// qjackspa.cpp - simple Qt4 LADSPA host for the Jack Audio Connection Kit
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

#include <iostream>
#include <exception>
#include "qjackspa.h"

#define PROGRAM_NAME "qjackspa"
#include "interface.c"
#define PROGRAM_DESCRIPTION_EXTRA PROGRAM_NAME " also accepts standard Qt options.\n\n"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	/* Command line options */
	GError *error = NULL;
	GOptionContext *context = interface_context();
	g_option_context_set_description( context, g_strconcat
	  ( PROGRAM_DESCRIPTION_EXTRA,
	    g_option_context_get_description(context), NULL ) );
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		QMessageBox::critical
		  ( 0, QString(PACKAGE_NAME " error"),
		    QString("Option parsing failed"), QMessageBox::Ok );
		std::cerr << "option parsing failed: " << error->message << std::endl;
		return -1;
	}

	/* Main window */
	QScrollArea window;

	/* Initialise jackspa */
	state_t state;
	if (!jackspa_init(&state, argc, argv)) {
		if (verbose)
			QMessageBox::critical
			  ( 0, QString(PACKAGE_NAME " error"),
			    QString("Cannot initialise jackspa"), QMessageBox::Ok );
		return 1;
	}

	try
	{

	/* Main layout */
	window.setWidget(new QWidget);
	QVBoxLayout *slider_box = new QVBoxLayout(window.widget());

	/* Title bar */
	QPushButton *button;
	QPushButton *toggle;
	{
		QGridLayout *title = 0;
		slider_box->addLayout(title = new QGridLayout);
		title->addWidget(button = new QPushButton("&Def"), 0, 0);
		title->addWidget(toggle = new QPushButton("&Xch"), 1, 0);
		QLabel *label = 0;
		title->addWidget(label = new QLabel(state.descriptor->Name), 0, 1, 2, 1);

		button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		toggle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		title->setHorizontalSpacing(4);
		title->setVerticalSpacing(2);
	}
	/* Initialise the controls */
	for (unsigned long p = 0, c = 0; p < state.descriptor->PortCount; p++)
		if ( LADSPA_IS_PORT_INPUT(state.descriptor->PortDescriptors[p]) &&
		     LADSPA_IS_PORT_CONTROL(state.descriptor->PortDescriptors[p]) )
		{
			ControlLayout *control = 0;
			slider_box->addLayout(control = new ControlLayout(&state, p, c++));
			QObject::connect( button, SIGNAL(clicked()),
			                  control, SLOT(on_button_pressed()) );
			QObject::connect( toggle, SIGNAL(clicked()),
			                  control, SLOT(exchange_control()) );
		}

	/* Main layout */
	slider_box->setContentsMargins(1, 1, 1, 1);
	slider_box->setSpacing(8);
	window.setWidgetResizable(true);
	window.frameSize().setWidth(400);
	window.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	window.setWindowTitle
	  ((*new QString(PROGRAM_NAME "_")).append(state.descriptor->Label));
	window.show();

	}
	catch(std::exception const &e)
	{
		QMessageBox::critical
		  ( 0, QString(PROGRAM_NAME),
		    QString("Exception failure while initialising the Qt interface:\n").
		      append(e.what()),
		    QMessageBox::Ok );
		throw(e);
	}

	int rc = app.exec();

	jackspa_fini(&state);
	return rc;
}

ControlLayout::ControlLayout(state_t *state, unsigned long port,
                             unsigned long ctrl,  QWidget *parent) :
	QGridLayout(parent),
	button("Def"),
	slider(Qt::Horizontal)
{
	control_init(&control, state, port, ctrl);
	label.setText(control.name);
	number.setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
	number.setAccelerated(true);
	number.setDecimals(4);
	number.setRange( static_cast<double>(control.min),
	                 static_cast<double>(control.max) );
	number.setSingleStep(static_cast<double>(control.inc.fine));
	slider.setTracking(false);
	slider.setRange(0, 5000);
	slider.setSingleStep
	  (5000 / nearbyintf((control.max - control.min) / control.inc.fine));
	slider.setPageStep
	  (5000 / nearbyintf((control.max - control.min) / control.inc.coarse));
	if (control.type == JACKSPA_INT || control.type == JACKSPA_TOGGLE) {
		number.setDecimals(0);
		slider.setRange(0, nearbyintf(control.max - control.min));
		slider.setSingleStep(1);
		slider.setPageStep(2);
		slider.setTickInterval(1);
		slider.setTickPosition(QSlider::TicksLeft);
	}

	/* Setup the widgets' state */
	number.setValue(static_cast<double>(*control.val));
	on_number_changed();

	/* Layout the widgets */
	setHorizontalSpacing(4);
	setVerticalSpacing(2);
	addWidget(&button, 0, 0);
	addWidget(&number, 1, 0);
	addWidget(&label, 0, 2);
	addWidget(&slider, 1, 2);
	button.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	label.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	slider.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	/* Signal connections */
	QObject::connect( &number, SIGNAL(valueChanged(double)),
	                  this, SLOT(on_number_changed()) );
	QObject::connect( &slider, SIGNAL(valueChanged(int)),
	                  this, SLOT(on_slider_changed()) );
	QObject::connect( &button, SIGNAL(clicked()),
	                  this, SLOT(on_button_pressed()) );
}

ControlLayout::~ControlLayout()
{
	control_fini(&control);
}

void ControlLayout::on_number_changed()
{
	*control.val = static_cast<LADSPA_Data>(number.value());
	set_slider(static_cast<LADSPA_Data>(number.value()));
}

void ControlLayout::on_slider_changed()
{
	int num_pos = slider.maximum() - slider.minimum();
	LADSPA_Data val = control.min +
	  static_cast<LADSPA_Data>(slider.value()) /
	  static_cast<LADSPA_Data>(num_pos) *
	  (control.max - control.min);

	*control.val = val;
	number.setValue(static_cast<double>(val));
}

void ControlLayout::on_button_pressed()
{
	if (control.def) {
		*control.val = *control.def;
		number.setValue(static_cast<double>(*control.def));
		set_slider(*control.def);
	}
}

void ControlLayout::exchange_control()
{
	control_exchange(&control);
	set_slider(*control.val);
}

void ControlLayout::set_slider(LADSPA_Data val)
{
	int num_pos = slider.maximum() - slider.minimum();

	slider.setValue( static_cast<int>( lrintf
	  ( ((val - control.min) * static_cast<LADSPA_Data>(num_pos)) /
	    (control.max - control.min) ) ) );
	  // setValue() will not emit valueChanged() if it is being set to its
	  // current value
}
