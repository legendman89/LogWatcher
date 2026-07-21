#pragma once

#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <filesystem>
#include <unordered_map>

#include "logger.hpp"
#include "utils.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Trans {

    inline std::string GetTranslationPath() {
        const auto root = std::filesystem::path(REL::Module::get().filename()).parent_path();
        return (root / "Data" / "SKSE" / "Plugins" / PRODUCT_NAME / TRANS_DIR / PRODUCT_NAME TRANS_DIR ".json").string();
    }

    class Translator {

        std::unordered_map<std::string, std::string> table;

    public:

        bool load() {

            const auto path = GetTranslationPath();

            table.clear();

            std::ifstream in(path);
            if (!in) {
                logger::error("Translation: could not open file {}", Utils::toUTF8(path));
                return false;
            }

            try {
                json data = json::parse(in, nullptr, true, true);

                if (!data.is_object()) {
                    logger::error("Translation: JSON root is not an object in file {}", Utils::toUTF8(path));
                    return false;
                }

                for (const auto& [k, v] : data.items()) {
                    if (v.is_string()) {
                        table.emplace(k, v.get<std::string>());
                    }
                }
            }
            catch (const std::exception& e) {
                logger::error("Translation: JSON parse error in file {}: {}", Utils::toUTF8(path), e.what());
                table.clear();
                return false;
            }

            return true;
        }

        const std::string& get(const std::string& key) {
            auto it = table.find(key);
            if (it != table.end()) {
                return it->second;
            }
			return table.emplace(key, key).first->second;
        }

    };

	inline Translator& GetTranslator() {
        static Translator translator;
        return translator;
	}

    inline const std::string& Tr(const std::string& key) {
        return GetTranslator().get(key);
    }

    inline std::string Tr(const std::string& key, const int& n) {
		std::string s = Tr(key);
		Utils::replaceAll(s, "{n}", std::to_string(n));
		return s;
    }

}
