#include "live.hpp"
#include "settings.hpp"
#include "restart.hpp"
#include "loading.hpp"

void Live::LogWatcherUI::RenderSettings()
{
	const auto rs = Logwatch::watcher.getRunState();
	auto& st = Logwatch::GetSettings();

	static bool settings_init = false;
	static Logwatch::LogWatcherSettings prev_settings{};
	static const Logwatch::LogWatcherSettings factory{};
	if (!settings_init) { prev_settings = st; settings_init = true; }

	int pushes = 0;
	AdjustBorder(pushes);
	SolidBackground(ImGuiCol_ChildBg);
	if (ImGui::BeginChild("lw_settings", ImVec2(0, 0), ImGuiChildFlags_Border)) {

		ImGui::Dummy(ImVec2(0, 4));
		if (rs == Logwatch::RunState::Stopped) {
			if (Live::CTAButton("Resume", true)) {
				st.pauseWatcher = false;
				Logwatch::watcher.resetFirstPoll();
				Logwatch::watcher.setRunState(Logwatch::RunState::Running);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(
					"Resume real-time log watching using the current settings."
				);
				ImGui::EndTooltip();
			}
		}
		else if (rs == Logwatch::RunState::AutoStopPending) {
			Live::CTAButton("Pausing...", false);
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(
					"Watcher is finishing the current scan then will pause.\n"
					"You can resume once it has fully stopped.");
				ImGui::EndTooltip();
			}
		}
		else if (rs == Logwatch::RunState::Running) {
			if (Live::CTAButton("Pause", true)) {
				st.pauseWatcher = true;
				const bool isPollDone = Logwatch::watcher.isFirstPollDone();
				if (isPollDone) {
					Logwatch::watcher.setRunState(Logwatch::RunState::Stopped);
				}
				else {
					Logwatch::watcher.setRunState(Logwatch::RunState::AutoStopPending);
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(
					"Pause watching after the current scan.\n"
					"Existing results stay visible, but no new live feed until you resume.\n"
					"This state is persistent on game save.\n"
				);
				ImGui::EndTooltip();
			}
		}
		ImGui::Dummy(ImVec2(0, 6));

		if (ImGui::CollapsingHeader("History", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::Checkbox("Deep scan (parse from beginning on next start)", &st.deepScan);
			Live::HelpMarker("When enabled, the watcher parses each tracked log from offset 0 on next (re)start. "
				"Otherwise, it tails from the current file end.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader("Cache", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::SliderInt("Cache capacity per mod", &st.cacheCap, 100, 20000);
			HelpMarker("How many recent messages per mod to keep in memory. "
				"This bounds what 'All' can show in the Details view.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader("Performance", 0)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::SliderInt("Poll interval (ms)", &st.pollIntervalMs, 100, 5000);
			HelpMarker("How often to check log files for new data. "
				"Lower values increase responsiveness but use more CPU.");
			ImGui::SliderInt("Max chunk per poll (KB)", &st.maxChunkKB, 16, (1 << 13));
			HelpMarker("Maximum data read from each log file per poll. "
				"Larger chunks reduce I/O calls but may cause hitches if too large.");
			ImGui::SliderInt("Max line length (KB)", &st.maxLineKB, 1, 1024);
			HelpMarker("Lines longer than this are skipped. "
				"Some logs may have very long lines; adjust as needed.");
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::Checkbox("Watch Papyrus logs", &st.watchPapyrus);
			HelpMarker("Enable monitoring of Papyrus script logs. "
				"These logs can be large and may impact performance.");
			ImGui::BeginDisabled(!st.watchPapyrus);
			ImGui::SliderInt("Max Papyrus chunk per poll (KB)", &st.papyrusMaxChunkKB, 256, (1 << 14));
			HelpMarker("Papyrus logs can be large; larger chunks help keep up.");
			ImGui::SliderInt("Max Papyrus line length (KB)", &st.papyrusMaxLineKB, 64, 2048);
			HelpMarker("Maximum line length for Papyrus logs.");
			ImGui::EndDisabled();
			ImGui::Dummy(ImVec2(0, 2));
			ImGui::TextDisabled("Papyrus logs often exceed 1 MB; larger chunks prevent lag.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader("Adaptive Boost (advanced)", 0)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::Checkbox("Enable auto-boost when behind", &st.autoBoostOnBacklog);
			HelpMarker("When the watcher falls behind (e.g. large log writes), "
				"it can temporarily increase chunk size to catch up faster.");
			ImGui::SliderInt("Backlog threshold (MB)", reinterpret_cast<int*>(&st.backlogBoostThresholdMB), 1, 256);
			HelpMarker("If the unread backlog exceeds this size, boosting activates.");
			ImGui::SliderFloat("Boost factor", &st.backlogBoostFactor, 1.0f, 4.0f, "%.1fx");
			HelpMarker("Multiplier for chunk size during boosting.");
			ImGui::SliderInt("Boost cap (MB)", reinterpret_cast<int*>(&st.maxBoostCapMB), 1, 32);
			HelpMarker("Maximum chunk size during boosting.");
			ImGui::Dummy(ImVec2(0, 2));
			ImGui::TextDisabled("When backlog exceeds threshold, chunk size grows temporarily to catch up.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader("UX (optional)", 0)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::Checkbox("Persist pins to disk", &st.persistPins);
			HelpMarker("When enabled, pinned mods are saved to disk and restored on restart.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader("Notifications System", 0)) {
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::Checkbox("Enable notifications / mailbox", &st.notificationsEnabled);
			HelpMarker("When enabled, the Watcher schedules HUD notifications and/or mailbox messages.");

			ImGui::SliderInt("HUD notification font scale (percentage)", &st.HUDFontScale, 50, 200);
			HelpMarker("Scale up or down the HUD font size");
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::SliderInt("HUD notification startup delay (sec)", &st.HUDPostLoadDelaySec, 2, 20);
			HelpMarker("After loading a (new) save, wait at least this long before showing notifications");
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::SliderInt("HUD notification hold-up (sec)", &st.HUDStayOnSec, 1, 10);
			HelpMarker("After firing an HUD notification, wait at least this long before fading out");
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::SliderInt("HUD notification fade-out (sec)", &st.HUDFadeOutSec, 0, 10);
			HelpMarker("Gradually fades out the the HUD notification");
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::SliderInt("HUD notification cooldown (sec)", &st.HUDDelaySec, 1, 10);
			HelpMarker("After firing an HUD notification, wait at least this long before another one");
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::BeginDisabled(!st.notificationsEnabled);

			ImGui::Checkbox("Enable periodic summaries", &st.periodicSummaryEnabled);
			HelpMarker("Every N seconds, summarize new issues since the last summary.\n"
				"Useful when you want a high-level view of how unstable your load order is.");
			ImGui::BeginDisabled(!st.periodicSummaryEnabled);

			ImGui::SliderInt("Periodic interval (sec)", &st.periodicIntervalSec, 60, 3600);
			HelpMarker("How often to emit a periodic summary.\n"
				"Shorter intervals give more frequent updates, longer intervals are quieter.");

			ImGui::SliderInt("Max mods listed per periodic summary", &st.periodicMaxMods, 1, 10);
			HelpMarker("Maximum number of individual mods briefed in each periodic summary.\n"
				"The rest are grouped as \"+N more\".");

			static const char* levelItems[] = {
				"Errors only",
				"Errors + warnings",
				"Errors + warnings + fails"
			};

			ImGui::Combo("Minimum level (periodic alerts)", &st.periodicMinLevel, levelItems, 3);
			HelpMarker("Controls which log levels are considered in a summary.\n"
				"0 = only errors, 1 = errors + warnings, 2 = errors + warnings + fails.");

			ImGui::EndDisabled(); // periodic options
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::Checkbox("Enable pinned mod alerts", &st.pinnedAlertsEnabled);
			HelpMarker("When enabled, pinned mods are watched more closely.\n"
				"If a pinned mod produces new issues, a short notification is generated\n"
				"and a detailed entry is added to the Mailbox tab.");

			ImGui::BeginDisabled(!st.pinnedAlertsEnabled);

			ImGui::SliderInt("Min new issues to alert", &st.pinnedMinNewIssues, 1, 50);
			HelpMarker("Minimum number of new issues from a pinned mod before an alert is raised.\n"
				"Higher values reduce spam on noisy mods.");

			ImGui::SliderInt("Per-mod cooldown (sec)", &st.pinnedAlertCooldownSec, 10, 3600);
			HelpMarker("After alerting for a pinned mod, wait at least this long before\n"
				"alerting again for the same mod.");

			ImGui::Combo("Minimum level (pinned alerts)", &st.pinnedMinLevel, levelItems, 3);
			HelpMarker("0 = only errors, 1 = errors + warnings, 2 = errors + warnings + fails.");

			ImGui::EndDisabled(); // pinned options

			ImGui::EndDisabled(); // all notifications
			ImGui::Dummy(ImVec2(0, 4));
		}


		ImGui::Separator();

		// Applying state
		static BusyState busyState = BusyState::Idle;
		const bool inflight = Logwatch::Restart::ApplyOnTheFly();
		const bool finished = Logwatch::Restart::ApplyDone();

		// Drop pauseWatcher from comparison
		auto stApply = st; stApply.pauseWatcher = false;
		auto prevApply = prev_settings; prevApply.pauseWatcher = false;
		auto factoryApply = factory; factoryApply.pauseWatcher = false;

		const bool canApply = (stApply != prevApply) && busyState != BusyState::Working;
		const bool canDefaults = (stApply != factoryApply) && busyState != BusyState::Working;

		if (Live::CTAButton("Save and Apply", canApply)) {
			busyState = BusyState::Working;
			Logwatch::applyNow();
		}
		ImGui::SameLine(0.0f, 6.0f);
		if (Live::CTAButton("Load Defaults", canDefaults)) {
			busyState = BusyState::Working;
			Logwatch::loadDefaults(factory);
			Logwatch::applyNow();
		}

		// status widget on same line
		ImGui::SameLine(0.0f, 20.0f);

		static BusyContext ui_ctx{};
		const double now = ImGui::GetTime();

		// Transition to Working state
		if (ui_ctx.state == BusyState::Idle && busyState == BusyState::Working) {
			ui_ctx.state = BusyState::Working;
			ui_ctx.t0 = now;
		}

		// If a new apply occurs during Done fade, restart animation
		if (ui_ctx.state == BusyState::Done && busyState == BusyState::Working) {
			ui_ctx.state = BusyState::Working;
			ui_ctx.t0 = now;
		}

		// Transition to Done state
		if (ui_ctx.state == BusyState::Working && !inflight && finished) {
			prev_settings = st;
			ui_ctx.state = BusyState::Done;
			busyState = BusyState::Idle;
			ui_ctx.t0 = now;
		}

		// Render according to state
		const float elapsed = float(now - ui_ctx.t0);
		switch (ui_ctx.state) {
		case BusyState::Working: {
			Live::renderBusy("Applying", elapsed);
			break;
		}
		case BusyState::Done: {
			const bool faded = Live::renderDone(ui_ctx.state, elapsed);
			if (faded) busyState = BusyState::Idle;
			break;
		}
		default:
			break;
		}

	}
	ImGui::EndChild();
	ImGui::PopStyleVar(pushes);
	ImGui::PopStyleColor();
}