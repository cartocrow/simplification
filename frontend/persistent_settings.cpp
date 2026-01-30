#include "persistent_settings.h"

#include <fstream>

PersistentSettings::PersistentSettings(std::string settings) {
	m_path = settings + ".json";
	std::ifstream f(m_path);
	if (!f.fail()) {
		f >> m_json;
	}
	m_changed = false;
}

PersistentSettings::~PersistentSettings() {
	if (m_changed) {
		std::ofstream f(m_path);
		f << std::setw(4) << m_json << std::endl;
	}
}

void PersistentSettings::setColor(Key key, Color value) {
	m_json[key] = { value.r, value.g, value.b };
	m_changed = true;
}

Color PersistentSettings::getColor(Key key, Color default_value) {
	Json& obj = m_json[key];
	if (obj.is_null()) {
		return default_value;
	}
	else {
		std::vector<int> rgb = obj.get<std::vector<int>>();
		return Color{ rgb[0], rgb[1], rgb[2] };
	}
}

void PersistentSettings::setPath(Key key, std::filesystem::path value) {
	m_json[key] = value.string();
	m_changed = true;
}

std::filesystem::path PersistentSettings::getPath(Key key, std::filesystem::path default_value) {
	Json& obj = m_json[key];
	if (obj.is_null()) {
		return default_value;
	}
	else {
		return obj.get<std::string>();
	}
}

void PersistentSettings::setString(Key key, std::string value) {
	m_json[key] = value;
	m_changed = true;
}

std::string PersistentSettings::getString(Key key, std::string default_value) {
	Json& obj = m_json[key];
	if (obj.is_null()) {
		return default_value;
	}
	else {
		return obj.get<std::string>();
	}
}

void PersistentSettings::setBoolean(Key key, bool value) {
	m_json[key] = value;
	m_changed = true;
}

bool PersistentSettings::getBoolean(Key key, bool default_value) {
	Json& obj = m_json[key];
	if (obj.is_null()) {
		return default_value;
	}
	else {
		return obj.get<bool>();
	}
}

void PersistentSettings::setInteger(Key key, int value) {
	m_json[key] = value;
	m_changed = true;
}

int PersistentSettings::getInteger(Key key, int default_value) {
	Json& obj = m_json[key];
	if (obj.is_null()) {
		return default_value;
	}
	else {
		return obj.get<int>();
	}
}

void PersistentSettings::setDouble(Key key, double value) {
	m_json[key] = value;
	m_changed = true;
}

int PersistentSettings::getDouble(Key key, double default_value) {
	Json& obj = m_json[key];
	if (obj.is_null()) {
		return default_value;
	}
	else {
		return obj.get<double>();
	}
}
