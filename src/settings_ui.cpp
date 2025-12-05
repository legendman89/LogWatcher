#include "live.hpp"
#include "settings.hpp"
#include "restart.hpp"
#include "loading.hpp"
#include "translate.hpp"

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
			if (Live::CTAButton(Trans::Tr("Settings.Run.Resume.Label").c_str(), true)) {
				st.pauseWatcher = false;
				Logwatch::watcher.resetFirstPoll();
				Logwatch::watcher.setRunState(Logwatch::RunState::Running);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(Trans::Tr("Settings.Run.Resume.Tooltip").c_str());
				ImGui::EndTooltip();
			}
		}
		else if (rs == Logwatch::RunState::AutoStopPending) {
			Live::CTAButton(Trans::Tr("Settings.Run.Pausing.Label").c_str(), false);
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(Trans::Tr("Settings.Run.Pausing.Tooltip").c_str());
				ImGui::EndTooltip();
			}
		}
		else if (rs == Logwatch::RunState::Running) {
			if (Live::CTAButton(Trans::Tr("Settings.Run.Pause.Label").c_str(), true)) {
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
				ImGui::TextUnformatted(Trans::Tr("Settings.Run.Pause.Tooltip").c_str());
				ImGui::EndTooltip();
			}
		}
		ImGui::Dummy(ImVec2(0, 6));

		if (ImGui::CollapsingHeader(Trans::Tr("Settings.History.Header").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::Checkbox(Trans::Tr("Settings.History.DeepScan.Label").c_str(), &st.deepScan);
			Live::HelpMarker(Trans::Tr("Settings.History.DeepScan.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader(Trans::Tr("Settings.Cache.Header").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::SliderInt(Trans::Tr("Settings.Cache.Capacity.Label").c_str(), &st.cacheCap, 100, 20000);
			HelpMarker(Trans::Tr("Settings.Cache.Capacity.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader(Trans::Tr("Settings.Performance.Header").c_str(), 0)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::SliderInt(Trans::Tr("Settings.Performance.PollInterval.Label").c_str(), &st.pollIntervalMs, 100, 5000);
			HelpMarker(Trans::Tr("Settings.Performance.PollInterval.Tooltip").c_str());
			ImGui::SliderInt(Trans::Tr("Settings.Performance.MaxChunk.Label").c_str(), &st.maxChunkKB, 16, (1 << 13));
			HelpMarker(Trans::Tr("Settings.Performance.MaxChunk.Tooltip").c_str());
			ImGui::SliderInt(Trans::Tr("Settings.Performance.MaxLine.Label").c_str(), &st.maxLineKB, 1, 1024);
			HelpMarker(Trans::Tr("Settings.Performance.MaxLine.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::Checkbox(Trans::Tr("Settings.Performance.WatchPapyrus.Label").c_str(), &st.watchPapyrus);
			HelpMarker(Trans::Tr("Settings.Performance.WatchPapyrus.Tooltip").c_str());
			ImGui::BeginDisabled(!st.watchPapyrus);
			ImGui::SliderInt(Trans::Tr("Settings.Performance.PapyrusChunk.Label").c_str(), &st.papyrusMaxChunkKB, 256, (1 << 14));
			HelpMarker(Trans::Tr("Settings.Performance.PapyrusChunk.Tooltip").c_str());
			ImGui::SliderInt(Trans::Tr("Settings.Performance.PapyrusLine.Label").c_str(), &st.papyrusMaxLineKB, 64, 2048);
			HelpMarker(Trans::Tr("Settings.Performance.PapyrusLine.Tooltip").c_str());
			ImGui::EndDisabled();
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader(Trans::Tr("Settings.Adaptive.Header").c_str(), 0)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::Checkbox(Trans::Tr("Settings.Adaptive.AutoBoost.Label").c_str(), &st.autoBoostOnBacklog);
			HelpMarker(Trans::Tr("Settings.Adaptive.AutoBoost.Tooltip").c_str());
			ImGui::SliderInt(Trans::Tr("Settings.Adaptive.Threshold.Label").c_str(), &st.backlogBoostThresholdMB, 1, 256);
			HelpMarker(Trans::Tr("Settings.Adaptive.Threshold.Tooltip").c_str());
			ImGui::SliderFloat(Trans::Tr("Settings.Adaptive.Factor.Label").c_str(), &st.backlogBoostFactor, 1.0f, 4.0f, "%.1fx");
			HelpMarker(Trans::Tr("Settings.Adaptive.Factor.Tooltip").c_str());
			ImGui::SliderInt(Trans::Tr("Settings.Adaptive.Cap.Label").c_str(), &st.maxBoostCapMB, 1, 32);
			HelpMarker(Trans::Tr("Settings.Adaptive.Cap.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader(Trans::Tr("Settings.UX.Header").c_str(), 0)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::Checkbox(Trans::Tr("Settings.UX.PersistPins.Label").c_str(), &st.persistPins);
			HelpMarker(Trans::Tr("Settings.UX.PersistPins.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader(Trans::Tr("Settings.Notifications.Header").c_str(), 0)) {
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::Checkbox(Trans::Tr("Settings.Notifications.Enabled.Label").c_str(), &st.notificationsEnabled);
			HelpMarker(Trans::Tr("Settings.Notifications.Enabled.Tooltip").c_str());

			ImGui::SliderInt(Trans::Tr("Settings.Notifications.HUDFontScale.Label").c_str(), &st.HUDFontScale, 50, 200);
			HelpMarker(Trans::Tr("Settings.Notifications.HUDFontScale.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::SliderInt(Trans::Tr("Settings.Notifications.PostLoadDelay.Label").c_str(), &st.HUDPostLoadDelaySec, 2, 20);
			HelpMarker(Trans::Tr("Settings.Notifications.PostLoadDelay.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::SliderInt(Trans::Tr("Settings.Notifications.StayOn.Label").c_str(), &st.HUDStayOnSec, 1, 10);
			HelpMarker(Trans::Tr("Settings.Notifications.StayOn.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::SliderInt(Trans::Tr("Settings.Notifications.FadeOut.Label").c_str(), &st.HUDFadeOutSec, 0, 10);
			HelpMarker(Trans::Tr("Settings.Notifications.FadeOut.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::SliderInt(Trans::Tr("Settings.Notifications.Cooldown.Label").c_str(), &st.HUDDelaySec, 1, 10);
			HelpMarker(Trans::Tr("Settings.Notifications.Cooldown.Tooltip").c_str());
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::BeginDisabled(!st.notificationsEnabled);

			ImGui::Checkbox(Trans::Tr("Settings.Notifications.PeriodicEnabled.Label").c_str(), &st.periodicSummaryEnabled);
			HelpMarker(Trans::Tr("Settings.Notifications.PeriodicEnabled.Tooltip").c_str());
			ImGui::BeginDisabled(!st.periodicSummaryEnabled);

			ImGui::SliderInt(Trans::Tr("Settings.Notifications.PeriodicInterval.Label").c_str(), &st.periodicIntervalSec, 60, 3600);
			HelpMarker(Trans::Tr("Settings.Notifications.PeriodicInterval.Tooltip").c_str());

			ImGui::SliderInt(Trans::Tr("Settings.Notifications.PeriodicMaxMods.Label").c_str(), &st.periodicMaxMods, 1, 10);
			HelpMarker(Trans::Tr("Settings.Notifications.PeriodicMaxMods.Tooltip").c_str());

			std::string per0 = Trans::Tr("Settings.Notifications.PeriodicMinLevel.Option0");
			std::string per1 = Trans::Tr("Settings.Notifications.PeriodicMinLevel.Option1");
			std::string per2 = Trans::Tr("Settings.Notifications.PeriodicMinLevel.Option2");
			const char* levelItemsPeriodic[] = {
				per0.c_str(),
				per1.c_str(),
				per2.c_str()
			};

			ImGui::Combo(Trans::Tr("Settings.Notifications.PeriodicMinLevel.Label").c_str(), &st.periodicMinLevel, levelItemsPeriodic, 3);
			HelpMarker(Trans::Tr("Settings.Notifications.PeriodicMinLevel.Tooltip").c_str());

			ImGui::EndDisabled();
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::Checkbox(Trans::Tr("Settings.Notifications.PinnedEnabled.Label").c_str(), &st.pinnedAlertsEnabled);
			HelpMarker(Trans::Tr("Settings.Notifications.PinnedEnabled.Tooltip").c_str());

			ImGui::BeginDisabled(!st.pinnedAlertsEnabled);

			ImGui::SliderInt(Trans::Tr("Settings.Notifications.PinnedMinIssues.Label").c_str(), &st.pinnedMinNewIssues, 1, 50);
			HelpMarker(Trans::Tr("Settings.Notifications.PinnedMinIssues.Tooltip").c_str());

			ImGui::SliderInt(Trans::Tr("Settings.Notifications.PinnedCooldown.Label").c_str(), &st.pinnedAlertCooldownSec, 10, 3600);
			HelpMarker(Trans::Tr("Settings.Notifications.PinnedCooldown.Tooltip").c_str());

			std::string pin0 = Trans::Tr("Settings.Notifications.PinnedMinLevel.Option0");
			std::string pin1 = Trans::Tr("Settings.Notifications.PinnedMinLevel.Option1");
			std::string pin2 = Trans::Tr("Settings.Notifications.PinnedMinLevel.Option2");
			const char* levelItemsPinned[] = {
				pin0.c_str(),
				pin1.c_str(),
				pin2.c_str()
			};

			ImGui::Combo(Trans::Tr("Settings.Notifications.PinnedMinLevel.Label").c_str(), &st.pinnedMinLevel, levelItemsPinned, 3);
			HelpMarker(Trans::Tr("Settings.Notifications.PinnedMinLevel.Tooltip").c_str());

			ImGui::EndDisabled();

			ImGui::EndDisabled();
			ImGui::Dummy(ImVec2(0, 4));
		}

		ImGui::Separator();

		static BusyState busyState = BusyState::Idle;
		const bool inflight = Logwatch::Restart::ApplyOnTheFly();
		const bool finished = Logwatch::Restart::ApplyDone();

		auto stApply = st; stApply.pauseWatcher = false;
		auto prevApply = prev_settings; prevApply.pauseWatcher = false;
		auto factoryApply = factory; factoryApply.pauseWatcher = false;

		const bool canApply = (stApply != prevApply) && busyState != BusyState::Working;
		const bool canDefaults = (stApply != factoryApply) && busyState != BusyState::Working;

		if (Live::CTAButton(Trans::Tr("Settings.Apply.SaveAndApply.Label").c_str(), canApply)) {
			busyState = BusyState::Working;
			Logwatch::applyNow();
		}
		ImGui::SameLine(0.0f, 6.0f);
		if (Live::CTAButton(Trans::Tr("Settings.Apply.LoadDefaults.Label").c_str(), canDefaults)) {
			busyState = BusyState::Working;
			Logwatch::loadDefaults(factory);
			Logwatch::applyNow();
		}

		ImGui::SameLine(0.0f, 20.0f);

		static BusyContext ui_ctx{};
		const double now = ImGui::GetTime();

		if (ui_ctx.state == BusyState::Idle && busyState == BusyState::Working) {
			ui_ctx.state = BusyState::Working;
			ui_ctx.t0 = now;
		}

		if (ui_ctx.state == BusyState::Done && busyState == BusyState::Working) {
			ui_ctx.state = BusyState::Working;
			ui_ctx.t0 = now;
		}

		if (ui_ctx.state == BusyState::Working && !inflight && finished) {
			prev_settings = st;
			ui_ctx.state = BusyState::Done;
			busyState = BusyState::Idle;
			ui_ctx.t0 = now;
		}

		const float elapsed = float(now - ui_ctx.t0);
		switch (ui_ctx.state) {
		case BusyState::Working: {
			Live::renderBusy(Trans::Tr("Settings.Apply.Applying.Status").c_str(), elapsed);
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
