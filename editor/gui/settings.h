// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QSettings>
#  include <QVariant>
#  include <QString>
#  include <QtWidgets>
#  include <QSignalBlocker>
#  include <color_selector.hpp> // from Qt color widgets
#include "warnpop.h"

#include <memory>
#include <string>

#include "data/fwd.h"
#include "editor/app/utility.h"
#include "editor/gui/utility.h"
#include "editor/gui/gfxwidget.h"

namespace gui
{
    // Settings wrapper
    class Settings
    {
    public:
        // Construct a new settings object for reading the
        // application "master" settings.
        Settings(const QString& organization, const QString& application);
        // Construct a new settings object for reading the
        // settings from a specific file. The contents are in JSON.
        Settings(const QString& filename);
        // Load the settings from the backing store.
        // Returns true if successful, otherwise false and the error is logged.
        bool Load();
        // Save the settings to the backing store.
        // Returns true if successful, otherwise false and the error is logged.
        bool Save();

        template<typename T>
        bool GetValue(const QString& module, const QString& key, T* out) const
        {
            const auto& value = mSettings->GetValue(module + "/" + key);
            if (!value.isValid())
                return false;
            *out = qvariant_cast<T>(value);
            return true;
        }

        bool GetValue(const QString& module, const QString& key, std::size_t* out) const;
        bool GetValue(const QString& module, const QString& key, std::string* out) const;
        bool GetValue(const QString& module, const QString& key, data::JsonObject* out) const;
        bool GetValue(const QString& module, const QString& key, QByteArray* out) const;

        // Get a value from the settings object under the specific key
        // under a specific module. If the module/key pair doesn't exist
        // then the default value is returned.
        template<typename T>
        T GetValue(const QString& module, const QString& key, const T& defaultValue) const
        {
            const auto& value = mSettings->GetValue(module + "/" + key);
            if (!value.isValid())
                return defaultValue;
            return qvariant_cast<T>(value);
        }

        QByteArray GetValue(const QString& mdoule, const QString& key, const QByteArray& defaultValue) const;
        std::string GetValue(const QString& module, const QString& key, const std::string& defaultValue) const;
        std::size_t GetValue(const QString& module, const QString& key, std::size_t defaultValue) const;


        // Set a value in the settings object under the specific module/key
        template<typename T>
        void SetValue(const QString& module, const QString& key, const T& value)
        { mSettings->SetValue(module + "/" + key, value); }
        void SetValue(const QString& module, const QString& key, const std::string& value)
        { SetValue(module, key, app::FromUtf8(value)); }
        void SetValue(const QString& module, const QString& key, std::size_t value)
        { SetValue<quint64>(module, key, quint64(value)); }

        void SetValue(const QString& module, const QString& key, const data::JsonObject& json);
        void SetValue(const QString& module, const QString& key, const QByteArray& bytes);

        // Save the state of a widget.
        void SaveWidget(const QString& module, const QTableView* table);
        void SaveWidget(const QString& module, const gui::GfxWidget* widget);
        void SaveWidget(const QString& module, const color_widgets::ColorSelector* color);
        void SaveWidget(const QString& module, const QComboBox* cmb);
        void SaveWidget(const QString& module, const QLineEdit* line);
        void SaveWidget(const QString& module, const QSpinBox* spin);
        void SaveWidget(const QString& module, const QDoubleSpinBox* spin);
        void SaveWidget(const QString& module, const QGroupBox* grp);
        void SaveWidget(const QString& module, const QCheckBox* chk);
        void SaveWidget(const QString& module, const QSplitter* splitter);

        // Load the state of a widget.
        void LoadWidget(const QString& module, gui::GfxWidget* widget) const;
        void LoadWidget(const QString& module, QSplitter* splitter) const;
        void LoadWidget(const QString& module, QTableView* table) const;
        void LoadWidget(const QString& module, QComboBox* cmb) const;
        void LoadWidget(const QString& module, QCheckBox* chk) const;
        void LoadWidget(const QString& module, QGroupBox* grp) const;
        void LoadWidget(const QString& module, QDoubleSpinBox* spin) const;
        void LoadWidget(const QString& module, QSpinBox* spin) const;
        void LoadWidget(const QString& module, QLineEdit* line) const;
        void LoadWidget(const QString& module, color_widgets::ColorSelector* selector) const;

        void SaveAction(const QString& module, const QAction* action);
        void LoadAction(const QString& module, QAction* action) const;

    private:
        class StorageImpl
        {
        public:
            virtual ~StorageImpl() = default;
            virtual QVariant GetValue(const QString& key) const = 0;
            virtual void SetValue(const QString& key, const QVariant& value) = 0;
            virtual bool Load() = 0;
            virtual bool Save() = 0;
        private:
        };
        // Settings storage implementation for accessing the
        // application settings through QSettings.
        class AppSettingsStorage : public StorageImpl
        {
        public:
            AppSettingsStorage(const QString& organization, const QString& application)
              : mSettings(organization, application)
            {}
            virtual QVariant GetValue(const QString& key) const override
            { return mSettings.value(key); }
            virtual void SetValue(const QString& key, const QVariant& value) override
            { mSettings.setValue(key, value); }
            virtual bool Load() override
            { return true; }
            virtual bool Save() override
            {
                mSettings.sync();
                return true;
            }
        private:
            QSettings mSettings;
        };
        // custom settings object that stores in JSON and can
        // be given a specific file.
        class JsonFileSettingsStorage : public StorageImpl
        {
        public:
            JsonFileSettingsStorage(const QString& file)
              : mFilename(file)
            {}
            virtual QVariant GetValue(const QString& key) const override
            { return mValues[key]; }
            virtual void SetValue(const QString& key, const QVariant& value) override;
            virtual bool Load() override;
            virtual bool Save() override;
        private:
            const QString mFilename;
            QVariantMap mValues;
        };

        std::unique_ptr<StorageImpl> mSettings;
    };
} // namespace