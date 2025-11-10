#pragma once

#include "logger.hpp"
#include <filesystem>
#include <ShlObj.h>
#pragma comment(lib, "Shell32.lib")

namespace Logwatch {

	inline std::filesystem::path GetDocumentsDir() {
		PWSTR wide = nullptr;
		std::filesystem::path out;
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &wide);
		if (SUCCEEDED(hr)) {
			out = std::filesystem::path(wide);
			CoTaskMemFree(wide);
		}
		else {
			logger::error("SHGetKnownFolderPath(FOLDERID_Documents) failed due to 0x{:X}", (unsigned)hr);
		}
		return out;
	}

}