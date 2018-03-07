// gjackspa.cpp - simple GTK+ LADSPA host for the Jack Audio Connection Kit
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

#include <gtkmm/main.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/window.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/scale.h>
#include "jackspa.h"
#include "control.h"

#define PROGRAM_NAME "gjackspa"
#include "interface.c"
using namespace Gtk;

class ControlWidget : public HBox
{
public:
	ControlWidget(state_t *state, unsigned long port, unsigned long ctrl);
	~ControlWidget();
	void on_button_pressed();
	void exchange_control();

protected:
	HScale slider;
	Label label;
	Button button;
	VBox box;
	control_t control;
	void on_value_changed();
};

int main(int argc, char **argv)
{
	Main kit(argc, argv,
	  *new Glib::OptionContext(interface_context(), true));

	state_t state;
	if (!jackspa_init(&state, argc, argv)) {
		if (verbose) {
			MessageDialog dialog( "Cannot initialise jackspa", false,
			                      MESSAGE_ERROR, BUTTONS_CLOSE, true );
			dialog.set_title(PACKAGE_NAME " error");
			dialog.set_position(WIN_POS_CENTER);
			dialog.run();
		}
		return 1;
	}

	VBox slider_box(true, 4);

	Button button("_Def", true);
	Button toggle("_Xch", true);
	{
		HBox *box = new HBox;
		box->set_spacing(10);
		VBox *button_box = new VBox;
		button_box->set_spacing(0);
		button_box->pack_start(button, PACK_SHRINK);
		button_box->pack_start(toggle, PACK_SHRINK);
		box->pack_start(*manage(button_box), PACK_SHRINK);
		box->pack_start
		  (*manage(new Label(state.descriptor->Name, 0.0, 0.5, false)), PACK_SHRINK);
		slider_box.pack_start(*manage(box), PACK_SHRINK);
	}
	for (unsigned long p = 0, c = 0; p < state.descriptor->PortCount; p++)
		if ( LADSPA_IS_PORT_INPUT(state.descriptor->PortDescriptors[p]) &&
		     LADSPA_IS_PORT_CONTROL(state.descriptor->PortDescriptors[p]) )
		{
			ControlWidget *control = new ControlWidget(&state, p, c++);
			slider_box.pack_start(*manage(control), PACK_SHRINK);

			button.signal_pressed().connect
			  (sigc::mem_fun(control, &ControlWidget::on_button_pressed));
			button.signal_activate().connect
			  (sigc::mem_fun(control, &ControlWidget::on_button_pressed));
			toggle.signal_pressed().connect
			  (sigc::mem_fun(control, &ControlWidget::exchange_control));
			toggle.signal_activate().connect
			  (sigc::mem_fun(control, &ControlWidget::exchange_control));
		}

	ScrolledWindow scroll;
	scroll.set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);
	scroll.set_border_width(1);
	scroll.add(slider_box);
	Window main_window;
	main_window.add(scroll);
	main_window.resize(400, main_window.get_height());
	main_window.set_title(state.client_name);
	main_window.show_all_children();

	Main::run(main_window);

	jackspa_fini(&state);

	return 0;
}

ControlWidget::ControlWidget(state_t *state, unsigned long port, unsigned long ctrl) :
	HBox(false, 10),
	label("", ALIGN_LEFT),
	button("Def"),
	box(false, 0)
{
	control_init(&control, state, port, ctrl);
	label.set_text(control.name);
	slider.set_range(control.min, control.max);
	slider.set_value(*control.val);
	slider.set_increments(control.inc.fine, control.inc.coarse);
	if (control.type == JACKSPA_INT || control.type == JACKSPA_TOGGLE)
		slider.set_digits(0);
	else
		slider.set_digits(2);

	slider.set_update_policy(UPDATE_DISCONTINUOUS);
	pack_start(button, PACK_SHRINK);
	pack_start(box, PACK_EXPAND_WIDGET);
	box.pack_start(label, PACK_EXPAND_WIDGET);
	box.pack_start(slider, PACK_EXPAND_WIDGET);
	slider.signal_value_changed().connect
	  (sigc::mem_fun(*this, &ControlWidget::on_value_changed));
	button.signal_pressed().connect
	  (sigc::mem_fun(*this, &ControlWidget::on_button_pressed));
}

ControlWidget::~ControlWidget()
{
	control_fini(&control);
}

void ControlWidget::on_value_changed()
{
	*control.val = slider.get_value();
}

void ControlWidget::on_button_pressed()
{
	if (control.def) slider.set_value(*control.def);
}

void ControlWidget::exchange_control()
{
	control_exchange(&control);
	slider.set_value(*control.val);
}
