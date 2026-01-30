#pragma once

#include <nlohmann/json.hpp>
#include <cartocrow/core/core.h>

using namespace cartocrow;

class PersistentSettings {
private:
	using Key = std::string;
	using Json = nlohmann::json;
public:
	PersistentSettings(std::string settings = "settings");
	~PersistentSettings();

	void setColor(Key key, Color value);
	Color getColor(Key key, Color default_value);

	void setPath(Key key, std::filesystem::path value);
	std::filesystem::path getPath(Key key, std::filesystem::path default_value);

	void setString(Key key, std::string value);
	std::string getString(Key key, std::string default_value);

	void setBoolean(Key key, bool value);
	bool getBoolean(Key key, bool default_value);

	void setInteger(Key key, int value);
	int getInteger(Key key, int default_value);

	void setDouble(Key key, double value);
	int getDouble(Key key, double default_value);
private:
	std::string m_path;
	Json m_json;
	bool m_changed;
};