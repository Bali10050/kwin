/*
    SPDX-FileCopyrightText: 2024 Jin Liu <m.liu.jin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kcmodule.h>

#include <QComboBox>

#include "ui_hidecursor_config.h"

class KActionCollection;

namespace KWin
{

class InactivityDurationComboBox : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(uint duration READ duration WRITE setDuration NOTIFY durationChanged)

public:
    explicit InactivityDurationComboBox(QWidget *parent = nullptr);

    uint duration() const;
    void setDuration(uint duration);

Q_SIGNALS:
    void durationChanged();
};

class HideCursorEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit HideCursorEffectConfig(QObject *parent, const KPluginMetaData &data);

public Q_SLOTS:
    void save() override;

private:
    Ui::HideCursorEffectConfig m_ui;
};

} // namespace
