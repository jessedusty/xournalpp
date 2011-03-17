#include "XournalWidget.h"
#include "../XournalView.h"
#include "../../util/Util.h"
#include "../Shadow.h"
#include "../../control/Control.h"
#include "../../control/settings/Settings.h"
#include "../../util/XInputUtils.h"
#include "../../cfg.h"

#include <math.h>

static void gtk_xournal_class_init(GtkXournalClass * klass);
static void gtk_xournal_init(GtkXournal * xournal);
static void gtk_xournal_size_request(GtkWidget * widget, GtkRequisition * requisition);
static void gtk_xournal_size_allocate(GtkWidget * widget, GtkAllocation * allocation);
static void gtk_xournal_realize(GtkWidget * widget);
static gboolean gtk_xournal_expose(GtkWidget * widget, GdkEventExpose * event);
static void gtk_xournal_destroy(GtkObject * object);
static void gtk_xournal_connect_scrollbars(GtkXournal * xournal);
static gboolean gtk_xournal_button_press_event(GtkWidget * widget, GdkEventButton * event);
static gboolean gtk_xournal_button_release_event(GtkWidget * widget, GdkEventButton * event);
static gboolean gtk_xournal_motion_notify_event(GtkWidget * widget, GdkEventMotion * event);
static void gtk_xournal_set_adjustment_upper(GtkAdjustment *adj, gdouble upper, gboolean always_emit_changed);

GtkType gtk_xournal_get_type(void) {
	static GtkType gtk_xournal_type = 0;

	if (!gtk_xournal_type) {
		static const GtkTypeInfo gtk_xournal_info = { "GtkXournal", sizeof(GtkXournal), sizeof(GtkXournalClass), (GtkClassInitFunc) gtk_xournal_class_init,
				(GtkObjectInitFunc) gtk_xournal_init, NULL, NULL, (GtkClassInitFunc) NULL };
		gtk_xournal_type = gtk_type_unique(GTK_TYPE_WIDGET, &gtk_xournal_info);
	}

	return gtk_xournal_type;
}

GtkWidget * gtk_xournal_new(XournalView * view, GtkRange * hrange, GtkRange * vrange) {
	GtkXournal * xoj = GTK_XOURNAL(gtk_type_new(gtk_xournal_get_type()));
	xoj->view = view;
	xoj->scrollX = 0;
	xoj->scrollY = 0;
	xoj->x = 0;
	xoj->y = 0;
	xoj->width = 10;
	xoj->height = 10;
	xoj->lastWidgetSize = 0;
	xoj->hrange = hrange;
	xoj->vrange = vrange;
	xoj->hadj = hrange->adjustment;
	xoj->vadj = vrange->adjustment;
	xoj->currentInputPage = NULL;
	xoj->lastInputPage = NULL;

	gtk_xournal_connect_scrollbars(xoj);

	return GTK_WIDGET(xoj);
}

static void gtk_xournal_vertical_scrolled(GtkAdjustment * adjustment, GtkXournal * xournal) {
	g_return_if_fail(GTK_IS_XOURNAL(xournal));

	xournal->y = gtk_adjustment_get_value(adjustment);

	gtk_widget_queue_draw(GTK_WIDGET(xournal));
}

void gtk_xournal_horizontal_scrolled(GtkAdjustment * adjustment, GtkXournal * xournal) {
	g_return_if_fail(GTK_IS_XOURNAL(xournal));

	xournal->x = gtk_adjustment_get_value(adjustment);

	gtk_widget_queue_draw(GTK_WIDGET(xournal));
}

void gtk_xournal_connect_scrollbars(GtkXournal * xournal) {
	g_return_if_fail(GTK_IS_ADJUSTMENT(xournal->hadj));
	g_return_if_fail(GTK_IS_ADJUSTMENT(xournal->vadj));

	g_signal_connect(xournal->hadj, "value-changed", G_CALLBACK(gtk_xournal_horizontal_scrolled), xournal);
	g_signal_connect(xournal->vadj, "value-changed", G_CALLBACK(gtk_xournal_vertical_scrolled), xournal);
}

void gtk_xournal_set_size(GtkWidget * widget, int width, int height) {
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_XOURNAL(widget));

	GtkXournal * xournal = GTK_XOURNAL(widget);

	xournal->width = width;
	xournal->height = height;

	g_return_if_fail(GTK_IS_ADJUSTMENT(xournal->hadj));
	g_return_if_fail(GTK_IS_ADJUSTMENT(xournal->vadj));

	gtk_adjustment_set_upper(xournal->hadj, width);
	gtk_adjustment_set_upper(xournal->vadj, height);
}

static void gtk_xournal_class_init(GtkXournalClass * klass) {
	GtkWidgetClass * widget_class;
	GtkObjectClass * object_class;

	widget_class = (GtkWidgetClass *) klass;
	object_class = (GtkObjectClass *) klass;

	widget_class->realize = gtk_xournal_realize;
	widget_class->size_request = gtk_xournal_size_request;
	widget_class->size_allocate = gtk_xournal_size_allocate;
	widget_class->button_press_event = gtk_xournal_button_press_event;
	widget_class->button_release_event = gtk_xournal_button_release_event;
	widget_class->motion_notify_event = gtk_xournal_motion_notify_event;
	widget_class->enter_notify_event = XInputUtils::onMouseEnterNotifyEvent;
	widget_class->leave_notify_event = XInputUtils::onMouseLeaveNotifyEvent;
	//	widget_class->scroll_event = gtk_xournal_scroll_event;

	widget_class->expose_event = gtk_xournal_expose;

	object_class->destroy = gtk_xournal_destroy;
}

gdouble gtk_xournal_get_wheel_delta(GtkRange * range, GdkScrollDirection direction) {
	GtkAdjustment * adj = range->adjustment;
	gdouble delta;

	if (GTK_IS_SCROLLBAR (range)) {
		delta = pow(adj->page_size, 2.0 / 3.0);
	} else {
		delta = adj->step_increment * 2;
	}

	if (direction == GDK_SCROLL_UP || direction == GDK_SCROLL_LEFT) {
		delta = -delta;
	}

	if (range->inverted) {
		delta = -delta;
	}

	return delta;
}

gboolean gtk_xournal_scroll_event(GtkWidget * widget, GdkEventScroll * event) {
#ifdef INPUT_DEBUG
	// true: Core event, false: XInput event
	gboolean isCore = (event->device == gdk_device_get_core_pointer());

	printf("DEBUG: Scroll (%s) (x,y)=(%.2f,%.2f), direction %d, modifier %x, isCore %i\n", event->device->name, event->x, event->y, event->direction,
			event->state, isCore);
#endif

	g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	GtkXournal * xournal = GTK_XOURNAL(widget);

	GtkRange * range;
	if (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_DOWN) {
		range = xournal->vrange;
	} else {
		range = xournal->hrange;
	}

	if (range && gtk_widget_get_visible(GTK_WIDGET(range))) {
		GtkAdjustment * adj = range->adjustment;

		double delta = gtk_xournal_get_wheel_delta(GTK_RANGE(range), event->direction);
		double new_value = CLAMP (adj->value + delta, adj->lower, adj->upper - adj->page_size);
		gtk_adjustment_set_value(adj, new_value);

		return true;
	}

	return false;
}

PageView * gtk_xournal_get_page_view_for_pos_cached(GtkXournal * xournal, int x, int y) {
	x += xournal->x;
	y += xournal->y;

	if (xournal->lastInputPage && xournal->lastInputPage->containsPoint(x, y)) {
		return xournal->lastInputPage;
	}

	PageView * pv = xournal->view->getViewAt(x, y);
	xournal->lastInputPage = pv;
	return pv;
}

gboolean gtk_xournal_button_press_event(GtkWidget * widget, GdkEventButton * event) {
#ifdef INPUT_DEBUG
	/**
	 * true: Core event, false: XInput event
	 */
	gboolean isCore = (event->device == gdk_device_get_core_pointer());

	printf("DEBUG: ButtonPress (%s) (x,y)=(%.2f,%.2f), button %d, modifier %x, isCore %i\n", event->device->name, event->x, event->y, event->button,
			event->state, isCore);
#endif
	XInputUtils::fixXInputCoords((GdkEvent*) event, widget);

	if (event->type != GDK_BUTTON_PRESS) {
		return false; // this event is not handled here
	}

	if (event->button > 3) { // scroll wheel events! don't paint...
		XInputUtils::handleScrollEvent(event, widget);
		return true;
	}

	gtk_widget_grab_focus(widget);

	GtkXournal * xournal = GTK_XOURNAL(widget);

	if (xournal->currentInputPage) {
		return false;
	}

	PageView * pv = gtk_xournal_get_page_view_for_pos_cached(xournal, event->x, event->y);
	if (pv) {

		// none button release event was sent, send one now
		if (xournal->currentInputPage) {
			GdkEventButton ev = *event;
			xournal->currentInputPage->translateEvent((GdkEvent*) &ev, xournal->x, xournal->y);
			xournal->currentInputPage->onButtonReleaseEvent(widget, &ev);
		}
		xournal->currentInputPage = pv;
		pv->translateEvent((GdkEvent*) event, xournal->x, xournal->y);
		return pv->onButtonPressEvent(widget, event);
	}

	return false; // not handled
}

gboolean gtk_xournal_button_release_event(GtkWidget * widget, GdkEventButton * event) {
#ifdef INPUT_DEBUG
	gboolean isCore = (event->device == gdk_device_get_core_pointer());
	printf("DEBUG: ButtonRelease (%s) (x,y)=(%.2f,%.2f), button %d, modifier %x, isCore %i\n", event->device->name, event->x, event->y, event->button,
			event->state, isCore);
#endif
	XInputUtils::fixXInputCoords((GdkEvent*) event, widget);

	GtkXournal * xournal = GTK_XOURNAL(widget);

	if (xournal->currentInputPage) {
		xournal->currentInputPage->translateEvent((GdkEvent*) event, xournal->x, xournal->y);
		bool res = xournal->currentInputPage->onButtonReleaseEvent(widget, event);
		xournal->currentInputPage = NULL;
		return res;
	}

	return false;
}

gboolean gtk_xournal_motion_notify_event(GtkWidget * widget, GdkEventMotion * event) {
#ifdef INPUT_DEBUG
	bool is_core = (event->device == gdk_device_get_core_pointer());
	printf("DEBUG: MotionNotify (%s) (x,y)=(%.2f,%.2f), modifier %x\n", is_core ? "core" : "xinput", event->x, event->y, event->state);
#endif

	XInputUtils::fixXInputCoords((GdkEvent*) event, widget);

	GtkXournal * xournal = GTK_XOURNAL(widget);

	PageView * pv = gtk_xournal_get_page_view_for_pos_cached(xournal, event->x, event->y);
	if (pv) {
		// allow events only to a single page!
		if (xournal->currentInputPage == NULL || pv == xournal->currentInputPage) {
			pv->translateEvent((GdkEvent*) event, xournal->x, xournal->y);
			return pv->onMotionNotifyEvent(widget, event);;
		}
	}

	return false;
}

static void gtk_xournal_init(GtkXournal * xournal) {
	GtkWidget * widget = GTK_WIDGET(xournal);

	GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

	int events = GDK_EXPOSURE_MASK;
	events |= GDK_POINTER_MOTION_MASK;
	events |= GDK_EXPOSURE_MASK;
	events |= GDK_BUTTON_MOTION_MASK;
	events |= GDK_BUTTON_PRESS_MASK;
	events |= GDK_BUTTON_RELEASE_MASK;
	events |= GDK_ENTER_NOTIFY_MASK;
	events |= GDK_LEAVE_NOTIFY_MASK;

	gtk_widget_set_events(widget, events);
}

// gtk_widget_get_preferred_size()
// the output don't interesset anybody...
static void gtk_xournal_size_request(GtkWidget * widget, GtkRequisition * requisition) {
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_XOURNAL(widget));
	g_return_if_fail(requisition != NULL);

	GtkXournal * xournal = GTK_XOURNAL(widget);

	requisition->width = 200;
	requisition->height = 200;
}

static void gtk_xournal_size_allocate(GtkWidget * widget, GtkAllocation * allocation) {
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_XOURNAL(widget));
	g_return_if_fail(allocation != NULL);

	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED(widget)) {
		gdk_window_move_resize(widget->window, allocation->x, allocation->y, allocation->width, allocation->height);
	}

	GtkXournal * xournal = GTK_XOURNAL(widget);

	if (xournal->lastWidgetSize != allocation->width) {
		// Layout pages calls gtk_xournal_set_size => so we can there update the scrollbars
		xournal->view->layoutPages();
		xournal->lastWidgetSize = allocation->width;
	}
	xournal->view->getControl()->calcZoomFitSize();

	xournal->hadj->page_size = allocation->width;
	xournal->hadj->page_increment = allocation->width * 0.9;
	xournal->hadj->lower = 0;
	// set_adjustment_upper() emits ::changed
	gtk_xournal_set_adjustment_upper(xournal->hadj, MAX(allocation->width, xournal->width), true);

	xournal->vadj->page_size = allocation->height;
	xournal->vadj->page_increment = allocation->height * 0.9;
	xournal->vadj->lower = 0;
	xournal->vadj->upper = MAX (allocation->height, xournal->height);
	gtk_xournal_set_adjustment_upper(xournal->vadj, MAX(allocation->height, xournal->height), true);
}

static void gtk_xournal_set_adjustment_upper(GtkAdjustment *adj, gdouble upper, gboolean always_emit_changed) {
	gboolean changed = FALSE;
	gboolean value_changed = FALSE;

	gdouble min = MAX (0., upper - adj->page_size);

	if (upper != adj->upper) {
		adj->upper = upper;
		changed = TRUE;
	}

	if (adj->value > min) {
		adj->value = min;
		value_changed = TRUE;
	}

	if (changed || always_emit_changed) {
		gtk_adjustment_changed(adj);
	}
	if (value_changed) {
		gtk_adjustment_value_changed(adj);
	}
}

static void gtk_xournal_realize(GtkWidget * widget) {
	GdkWindowAttr attributes;
	guint attributes_mask;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_XOURNAL(widget));

	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;

	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y;

	widget->window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributes_mask);
	gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &widget->style->dark[GTK_STATE_NORMAL]);

	gdk_window_set_user_data(widget->window, widget);

	widget->style = gtk_style_attach(widget->style, widget->window);
	gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

	gtk_xournal_update_xevent(widget);
}

void gtk_xournal_redraw(GtkWidget * widget) {
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_XOURNAL(widget));

	GtkXournal * xournal = GTK_XOURNAL(widget);

	GdkRegion * region = gdk_drawable_get_clip_region(GTK_WIDGET(xournal)->window);
	gdk_window_invalidate_region(GTK_WIDGET(xournal)->window, region, TRUE);
	gdk_window_process_updates(GTK_WIDGET(xournal)->window, TRUE);
}

/**
 * Change event handling between XInput and Core
 */
void gtk_xournal_update_xevent(GtkWidget * widget) {
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_XOURNAL(widget));

	GtkXournal * xournal = GTK_XOURNAL(widget);

	Settings * settings = xournal->view->getControl()->getSettings();

	if (!gtk_check_version(2, 17, 0)) {
		/* GTK+ 2.17 and later: everybody shares a single native window,
		 so we'll never get any core events, and we might as well set
		 extension events the way we're supposed to. Doing so helps solve
		 crasher bugs in 2.17, and prevents us from losing two-button
		 events in 2.18 */
		gtk_widget_set_extension_events(widget, settings->isUseXInput() ? GDK_EXTENSION_EVENTS_ALL : GDK_EXTENSION_EVENTS_NONE);
	} else {
		/* GTK+ 2.16 and earlier: we only activate extension events on the
		 PageViews's parent GdkWindow. This allows us to keep receiving core
		 events. */
		gdk_input_set_extension_events(widget->window, GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
				settings->isUseXInput() ? GDK_EXTENSION_EVENTS_ALL : GDK_EXTENSION_EVENTS_NONE);
	}

}

static void gtk_xournal_draw_shadow(GtkXournal * xournal, cairo_t * cr, int left, int top, int width, int height, bool selected) {
	if (selected) {
		Shadow::drawShadow(cr, left - 2, top - 2, width + 4, height + 4);

		Settings * settings = xournal->view->getControl()->getSettings();

		// Draw border
		Util::cairo_set_source_rgbi(cr, settings->getSelectionColor());
		cairo_set_line_width(cr, 4.0);
		cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
		cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);

		cairo_move_to(cr, left, top);
		cairo_line_to(cr, left, top + height);
		cairo_line_to(cr, left + width, top + height);
		cairo_line_to(cr, left + width, top);
		cairo_close_path(cr);

		cairo_stroke(cr);
	} else {
		Shadow::drawShadow(cr, left, top, width, height);
	}
}

static gboolean gtk_xournal_expose(GtkWidget * widget, GdkEventExpose * event) {
	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_XOURNAL(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	GtkXournal * xournal = GTK_XOURNAL(widget);

	cairo_t * cr = gdk_cairo_create(GTK_WIDGET(widget)->window);

	ArrayIterator<PageView *> it = xournal->view->pageViewIterator();

	while (it.hasNext()) {
		PageView * pv = it.next();

		int x = pv->getX() - xournal->x;
		int y = pv->getY() - xournal->y;

		gtk_xournal_draw_shadow(xournal, cr, x, y, pv->getDisplayWidth(), pv->getDisplayHeight(), pv->isSelected());
		cairo_save(cr);
		cairo_translate(cr, x, y);
		pv->paintPage(cr, NULL);
		cairo_restore(cr);
	}

	cairo_destroy(cr);

	return true;
}

static void gtk_xournal_destroy(GtkObject * object) {
	g_return_if_fail(object != NULL);
	g_return_if_fail(GTK_IS_XOURNAL(object));

	GtkXournal * xournal = GTK_XOURNAL(object);

	GtkXournalClass * klass = (GtkXournalClass *) gtk_type_class(gtk_widget_get_type());

	if (GTK_OBJECT_CLASS(klass)->destroy) {
		(*GTK_OBJECT_CLASS(klass)->destroy)(object);
	}
}
