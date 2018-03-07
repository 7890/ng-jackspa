// qjackspa.h - include file for qjackspa.cpp
// Copyright © 2013 Géraud Meyer <graud@gmx.com>
//
// This file is part of ng-jackspa.
//
// ng-jackspa is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 2 as published by the
// Free Software Foundation.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along
// with ng-jackspa.  If not, see <http://www.gnu.org/licenses/>.

#ifndef QJACKSPA_H
#define QJACKSPA_H

#include "control.h"
#include <cmath>
#include <QtGui/QWidget>
#include <QtGui/QApplication>
#include <QtGui/QMessageBox>
#include <QtGui/QScrollArea>
#include <QtGui/QGridLayout>
#include <QtGui/QPushButton>
#include <QtGui/QLabel>
#include <QtGui/QSpinBox>
#include <QtGui/QSlider>


class ControlLayout : public QGridLayout
{
	Q_OBJECT

public:
	ControlLayout(state_t *state, unsigned long port, unsigned long ctrl,
	              QWidget *parent = 0);
	~ControlLayout();

public slots:
	void on_button_pressed();

protected:
	control_t control;
	QPushButton button;
	QDoubleSpinBox number;
	QLabel label;
	QSlider slider;
	void set_slider(LADSPA_Data value);

protected slots:
	void on_number_changed();
	void on_slider_changed();
	void exchange_control();
};

#endif
