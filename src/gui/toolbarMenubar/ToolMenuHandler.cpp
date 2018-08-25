#include "ToolMenuHandler.h"

#include "FontButton.h"
#include "MenuItem.h"
#include "ToolButton.h"
#include "ToolDrawCombocontrol.h"
#include "ToolPageLayer.h"
#include "ToolPageSpinner.h"
#include "ToolSelectCombocontrol.h"
#include "ToolZoomSlider.h"

#include "control/Actions.h"
#include "gui/ToolitemDragDrop.h"
#include "gui/widgets/SelectColor.h"
#include "model/ToolbarData.h"
#include "model/ToolbarModel.h"

#include <config.h>
#include <config-features.h>
#include <i18n.h>

#include <glade/glade-xml.h>
#include <glib.h>

ToolMenuHandler::ToolMenuHandler(ActionHandler* listener, ZoomControl* zoom, GladeGui* gui, ToolHandler* toolHandler,
								 GtkWindow* parent)
{
	XOJ_INIT_TYPE(ToolMenuHandler);

	this->parent = parent;
	this->listener = listener;
	this->zoom = zoom;
	this->gui = gui;
	this->toolHandler = toolHandler;
	this->undoButton = NULL;
	this->redoButton = NULL;
	this->toolPageSpinner = NULL;
	this->toolPageLayer = NULL;
	this->tbModel = new ToolbarModel();

	initToolItems();
}

ToolMenuHandler::~ToolMenuHandler()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	delete this->tbModel;
	this->tbModel = NULL;

	for (MenuItem* it : this->menuItems)
	{
		delete it;
		it = NULL;
	}

	freeDynamicToolbarItems();

	for (AbstractToolItem* it : this->toolItems)
	{
		delete it;
		it = NULL;
	}

	XOJ_RELEASE_TYPE(ToolMenuHandler);
}

void ToolMenuHandler::freeDynamicToolbarItems()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	for (AbstractToolItem* it : this->toolItems)
	{
		it->setUsed(false);
	}

	for (ColorToolItem* it : this->toolbarColorItems)
	{
		delete it;
	}
	this->toolbarColorItems.clear();
}

void ToolMenuHandler::unloadToolbar(GtkWidget* toolbar)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	for (int i = gtk_toolbar_get_n_items(GTK_TOOLBAR(toolbar)) - 1; i >= 0; i--)
	{
		GtkToolItem* tbItem = gtk_toolbar_get_nth_item(GTK_TOOLBAR(toolbar), i);
		gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(tbItem));
	}

	gtk_widget_hide(toolbar);
}

void ToolMenuHandler::load(ToolbarData* d, GtkWidget* toolbar, const char* toolbarName, bool horizontal)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	int count = 0;

	for (ToolbarEntry* e : d->contents)
	{
		if (e->getName() == toolbarName)
		{
			for (ToolbarItem* dataItem : *e->getItems())
			{
				string name = dataItem->getName();

				if (name == "SEPARATOR")
				{
					GtkToolItem* it = gtk_separator_tool_item_new();
					gtk_widget_show(GTK_WIDGET(it));
					gtk_toolbar_insert(GTK_TOOLBAR(toolbar), it, -1);

					ToolitemDragDrop::attachMetadata(GTK_WIDGET(it), dataItem->getId(), TOOL_ITEM_SEPARATOR);

					continue;
				}

				if (name == "SPACER")
				{
					GtkToolItem* toolItem = gtk_separator_tool_item_new();
					gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(toolItem), false);
					gtk_tool_item_set_expand(toolItem, true);
					gtk_widget_show(GTK_WIDGET(toolItem));
					gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolItem, -1);

					ToolitemDragDrop::attachMetadata(GTK_WIDGET(toolItem), dataItem->getId(), TOOL_ITEM_SPACER);

					continue;
				}
				if (ba::starts_with(name, "COLOR(") && name.length() == 15)
				{
					string color = name.substr(6, 8);
					if (!ba::starts_with(color, "0x"))
					{
						g_warning("Toolbar:COLOR(...) has to start with 0x, get color: %s", color.c_str());
						continue;
					}
					count++;

					color = color.substr(2);
					gint c = g_ascii_strtoll(color.c_str(), NULL, 16);

					ColorToolItem* item = new ColorToolItem(listener, toolHandler, this->parent, c);
					this->toolbarColorItems.push_back(item);

					GtkToolItem* it = item->createItem(horizontal);
					gtk_widget_show_all(GTK_WIDGET(it));
					gtk_toolbar_insert(GTK_TOOLBAR(toolbar), it, -1);

					ToolitemDragDrop::attachMetadataColor(GTK_WIDGET(it), dataItem->getId(), c, item);

					continue;
				}

				bool found = false;
				for (AbstractToolItem* item : this->toolItems)
				{
					if (name == item->getId())
					{
						if (item->isUsed())
						{
							g_warning("You can use the toolbar item \"%s\" only once!", item->getId().c_str());
							found = true;
							continue;
						}
						item->setUsed(true);

						count++;
						GtkToolItem* it = item->createItem(horizontal);
						gtk_widget_show_all(GTK_WIDGET(it));
						gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(it), -1);

						ToolitemDragDrop::attachMetadata(GTK_WIDGET(it), dataItem->getId(), item);

						found = true;
						break;
					}
				}
				if (!found)
				{
					g_warning("Toolbar item \"%s\" not found!", name.c_str());
				}
			}

			break;
		}
	}

	if (count == 0)
	{
		gtk_widget_hide(toolbar);
	}
	else
	{
		gtk_widget_show(toolbar);
	}
}

void ToolMenuHandler::removeColorToolItem(AbstractToolItem* it)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	g_return_if_fail(it != NULL);
	for (unsigned int i = 0; i < this->toolbarColorItems.size(); i++)
	{
		if (this->toolbarColorItems[i] == it)
		{
			this->toolbarColorItems.erase(this->toolbarColorItems.begin() + i);
			break;
		}
	}
	delete (ColorToolItem*) it;
}

void ToolMenuHandler::addColorToolItem(AbstractToolItem* it)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	g_return_if_fail(it != NULL);
	this->toolbarColorItems.push_back((ColorToolItem*) it);
}

void ToolMenuHandler::setTmpDisabled(bool disabled)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	for (AbstractToolItem* it : this->toolItems) it->setTmpDisabled(disabled);
	for (MenuItem* it : this->menuItems) it->setTmpDisabled(disabled);
	for (ColorToolItem* it : this->toolbarColorItems) it->setTmpDisabled(disabled);

	GtkWidget* menuViewSidebarVisible = gui->get("menuViewSidebarVisible");
	gtk_widget_set_sensitive(menuViewSidebarVisible, !disabled);
}

void ToolMenuHandler::addToolItem(AbstractToolItem* it)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	this->toolItems.push_back(it);
}

void ToolMenuHandler::registerMenupoint(GtkWidget* widget, ActionType type)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	MenuItem* it = new MenuItem(listener, widget, type);
	this->menuItems.push_back(it);
}

void ToolMenuHandler::registerMenupoint(GtkWidget* widget, ActionType type, ActionGroup group)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	MenuItem* it = new MenuItem(listener, widget, type, group);
	this->menuItems.push_back(it);
}

void ToolMenuHandler::initEraserToolItem()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	ToolButton* tbEraser = new ToolButton(listener, gui, "ERASER", ACTION_TOOL_ERASER, GROUP_TOOL, true,
										  "tool_eraser.svg", _C("Eraser"), gui->get("menuToolsEraser"));
	GtkWidget* eraserPopup = gtk_menu_new();

	GtkWidget* eraserPopupStandard = gtk_check_menu_item_new_with_label(_C("standard"));
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(eraserPopupStandard), true);
	gtk_widget_show(eraserPopupStandard);
	gtk_container_add(GTK_CONTAINER(eraserPopup), eraserPopupStandard);
	registerMenupoint(gui->get("eraserStandard"), ACTION_TOOL_ERASER_STANDARD, GROUP_ERASER_MODE);
	registerMenupoint(eraserPopupStandard, ACTION_TOOL_ERASER_STANDARD, GROUP_ERASER_MODE);

	GtkWidget* eraserPopupWhiteout = gtk_check_menu_item_new_with_label(_C("whiteout"));
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(eraserPopupWhiteout), true);
	gtk_widget_show(eraserPopupWhiteout);
	gtk_container_add(GTK_CONTAINER(eraserPopup), eraserPopupWhiteout);
	registerMenupoint(gui->get("eraserWhiteout"), ACTION_TOOL_ERASER_WHITEOUT, GROUP_ERASER_MODE);
	registerMenupoint(eraserPopupWhiteout, ACTION_TOOL_ERASER_WHITEOUT, GROUP_ERASER_MODE);

	GtkWidget* eraserPopupDeleteStroke = gtk_check_menu_item_new_with_label(_C("delete stroke"));
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(eraserPopupDeleteStroke), true);
	gtk_widget_show(eraserPopupDeleteStroke);
	gtk_container_add(GTK_CONTAINER(eraserPopup), eraserPopupDeleteStroke);
	registerMenupoint(gui->get("eraserDeleteStrokes"), ACTION_TOOL_ERASER_DELETE_STROKE, GROUP_ERASER_MODE);
	registerMenupoint(eraserPopupDeleteStroke, ACTION_TOOL_ERASER_DELETE_STROKE, GROUP_ERASER_MODE);

	tbEraser->setPopupMenu(eraserPopup);

	addToolItem(tbEraser);
}

void ToolMenuHandler::initToolItems()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	addToolItem(new ToolButton(listener, "SAVE", ACTION_SAVE, GTK_STOCK_SAVE, _C("Save"), gui->get("menuFileSave")));
	addToolItem(new ToolButton(listener, "NEW", ACTION_NEW, GTK_STOCK_NEW, _C("New Xournal"), gui->get("menuFileNew")));

	addToolItem(new ToolButton(listener, "OPEN", ACTION_OPEN, GTK_STOCK_OPEN, _C("Open file"), gui->get("menuFileOpen")));

	addToolItem(new ToolButton(listener, "CUT", ACTION_CUT, GTK_STOCK_CUT, _C("Cut"), gui->get("menuEditCut")));
	addToolItem(new ToolButton(listener, "COPY", ACTION_COPY, GTK_STOCK_COPY, _C("Copy"), gui->get("menuEditCopy")));
	addToolItem(new ToolButton(listener, "PASTE", ACTION_PASTE, GTK_STOCK_PASTE, _C("Paste"), gui->get("menuEditPaste")));

	addToolItem(new ToolButton(listener, "SEARCH", ACTION_SEARCH, GTK_STOCK_FIND, _C("Search"), gui->get("menuEditSearch")));

	undoButton = new ToolButton(listener, "UNDO", ACTION_UNDO, GTK_STOCK_UNDO, _C("Undo"), gui->get("menuEditUndo"));
	redoButton = new ToolButton(listener, "REDO", ACTION_REDO, GTK_STOCK_REDO, _C("Redo"), gui->get("menuEditRedo"));
	addToolItem(undoButton);
	addToolItem(redoButton);

	addToolItem(new ToolButton(listener, "GOTO_FIRST", ACTION_GOTO_FIRST, GTK_STOCK_GOTO_FIRST, _C("Go to first page"),
							   gui->get("menuViewFirstPage")));
	addToolItem(new ToolButton(listener, "GOTO_BACK", ACTION_GOTO_BACK, GTK_STOCK_GO_BACK, _C("Back"),
							   gui->get("menuNavigationPreviousPage")));

	addToolItem(new ToolButton(listener, "GOTO_BACK", ACTION_GOTO_BACK, GTK_STOCK_GO_BACK, _C("Back"),
							   gui->get("menuNavigationPreviousPage")));

	addToolItem(new ToolButton(listener, gui, "GOTO_PAGE", ACTION_GOTO_PAGE, "goto.svg", _C("Go to page"),
							   gui->get("menuNavigationGotoPage")));

	addToolItem(new ToolButton(listener, "GOTO_NEXT", ACTION_GOTO_NEXT, GTK_STOCK_GO_FORWARD, _C("Next"),
							   gui->get("menuNavigationNextPage")));
	addToolItem(new ToolButton(listener, "GOTO_LAST", ACTION_GOTO_LAST, GTK_STOCK_GOTO_LAST, _C("Go to last page"),
							   gui->get("menuNavigationLastPage")));

	addToolItem(new ToolButton(listener, gui, "GOTO_NEXT_ANNOTATED_PAGE", ACTION_GOTO_NEXT_ANNOTATED_PAGE,
							   "nextAnnotatedPage.svg", _C("Next annotated page"),
							   gui->get("menuNavigationNextAnnotatedPage")));

	addToolItem(new ToolButton(listener, "ZOOM_OUT", ACTION_ZOOM_OUT, GTK_STOCK_ZOOM_OUT, _C("Zoom out"),
							   gui->get("menuViewZoomOut")));
	addToolItem(new ToolButton(listener, "ZOOM_IN", ACTION_ZOOM_IN, GTK_STOCK_ZOOM_IN, _C("Zoom in"),
							   gui->get("menuViewZoomIn")));
	addToolItem(new ToolButton(listener, "ZOOM_FIT", ACTION_ZOOM_FIT, GTK_STOCK_ZOOM_FIT, _C("Zoom fit to screen"),
							   gui->get("menuViewZoomFit")));
	addToolItem(new ToolButton(listener, "ZOOM_100", ACTION_ZOOM_100, GTK_STOCK_ZOOM_100, _C("Zoom to 100%"),
							   gui->get("menuViewZoom100")));

	addToolItem(new ToolButton(listener, gui, "FULLSCREEN", ACTION_FULLSCREEN, GROUP_FULLSCREEN, false,
							   "fullscreen.svg", _C("Toggle fullscreen"), gui->get("menuViewFullScreen")));

	addToolItem(new ColorToolItem(listener, toolHandler, this->parent, 0xff0000, true));

	addToolItem(new ToolButton(listener, gui, "PEN", ACTION_TOOL_PEN, GROUP_TOOL, true, "tool_pencil.svg", _C("Pen"),
							   gui->get("menuToolsPen")));

	initEraserToolItem();

	addToolItem(new ToolButton(listener, gui, "DELETE_CURRENT_PAGE", ACTION_DELETE_PAGE, "delPage.svg", _C("Delete current page")));

	addToolItem(new ToolSelectCombocontrol(this, listener, gui, "SELECT"));

	addToolItem(new ToolDrawCombocontrol(this, listener, gui, "DRAW"));

	ToolButton* tbInsertNewPage = new ToolButton(listener, gui, "INSERT_NEW_PAGE", ACTION_NEW_PAGE_AFTER,
												 "addPage.svg", _C("Insert page"));
	addToolItem(tbInsertNewPage);
	GtkWidget* newPagePopup = gtk_menu_new();

	GtkWidget* newPagePopupPlain = gtk_check_menu_item_new_with_label(_C("Plain"));
	gtk_widget_show(newPagePopupPlain);
	gtk_container_add(GTK_CONTAINER(newPagePopup), newPagePopupPlain);
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(newPagePopupPlain), true);
	registerMenupoint(newPagePopupPlain, ACTION_NEW_PAGE_PLAIN, GROUP_PAGE_INSERT_TYPE);
	registerMenupoint(gui->get("menuJournalNewPageAfterPlain"), ACTION_NEW_PAGE_PLAIN, GROUP_PAGE_INSERT_TYPE);

	GtkWidget* newPagePopupLined = gtk_check_menu_item_new_with_label(_C("Lined"));
	gtk_widget_show(newPagePopupLined);
	gtk_container_add(GTK_CONTAINER(newPagePopup), newPagePopupLined);
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(newPagePopupLined), true);
	registerMenupoint(newPagePopupLined, ACTION_NEW_PAGE_LINED, GROUP_PAGE_INSERT_TYPE);
	registerMenupoint(gui->get("menuJournalNewPageAfterLined"), ACTION_NEW_PAGE_LINED, GROUP_PAGE_INSERT_TYPE);

	GtkWidget* newPagePopupRuled = gtk_check_menu_item_new_with_label(_C("Ruled"));
	gtk_widget_show(newPagePopupRuled);
	gtk_container_add(GTK_CONTAINER(newPagePopup), newPagePopupRuled);
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(newPagePopupRuled), true);
	registerMenupoint(newPagePopupRuled, ACTION_NEW_PAGE_RULED, GROUP_PAGE_INSERT_TYPE);
	registerMenupoint(gui->get("menuJournalNewPageAfterRuled"), ACTION_NEW_PAGE_RULED, GROUP_PAGE_INSERT_TYPE);

	GtkWidget* newPagePopupGraph = gtk_check_menu_item_new_with_label(_C("Graph"));
	gtk_widget_show(newPagePopupGraph);
	gtk_container_add(GTK_CONTAINER(newPagePopup), newPagePopupGraph);
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(newPagePopupGraph), true);
	registerMenupoint(newPagePopupGraph, ACTION_NEW_PAGE_GRAPH, GROUP_PAGE_INSERT_TYPE);
	registerMenupoint(gui->get("menuJournalNewPageAfterGraph"), ACTION_NEW_PAGE_GRAPH, GROUP_PAGE_INSERT_TYPE);

	GtkWidget* newPagePopupCopyCurrent = gtk_check_menu_item_new_with_label(_C("Copy current"));
	gtk_widget_show(newPagePopupCopyCurrent);
	gtk_container_add(GTK_CONTAINER(newPagePopup), newPagePopupCopyCurrent);
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(newPagePopupCopyCurrent), true);
	registerMenupoint(newPagePopupCopyCurrent, ACTION_NEW_PAGE_COPY, GROUP_PAGE_INSERT_TYPE);
	registerMenupoint(gui->get("menuJournalNewPageAfterCopy"), ACTION_NEW_PAGE_COPY, GROUP_PAGE_INSERT_TYPE);

	GtkWidget* newPagePopupWithPDFBackground = gtk_check_menu_item_new_with_label(_C("With PDF background"));
	gtk_widget_show(newPagePopupWithPDFBackground);
	gtk_container_add(GTK_CONTAINER(newPagePopup), newPagePopupWithPDFBackground);
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(newPagePopupWithPDFBackground), true);
	registerMenupoint(newPagePopupWithPDFBackground, ACTION_NEW_PAGE_PDF_BACKGROUND, GROUP_PAGE_INSERT_TYPE);
	registerMenupoint(gui->get("menuJournalNewPageAfterWithPdf"), ACTION_NEW_PAGE_PDF_BACKGROUND, GROUP_PAGE_INSERT_TYPE);

	tbInsertNewPage->setPopupMenu(newPagePopup);

	addToolItem(new ToolButton(listener, gui, "HILIGHTER", ACTION_TOOL_HILIGHTER, GROUP_TOOL, true,
							   "tool_highlighter.png", _C("Hilighter"), gui->get("menuToolsHighlighter")));
	addToolItem(new ToolButton(listener, gui, "TEXT", ACTION_TOOL_TEXT, GROUP_TOOL, true,
							   "tool_text.svg", _C("Text"), gui->get("menuToolsText")));
	addToolItem(new ToolButton(listener, gui, "IMAGE", ACTION_TOOL_IMAGE, GROUP_TOOL, true,
							   "tool_image.svg", _C("Image"), gui->get("menuToolsImage")));

	addToolItem(new ToolButton(listener, gui, "SELECT_REGION", ACTION_TOOL_SELECT_REGION, GROUP_TOOL, true,
							   "lasso.svg", _C("Select Region"), gui->get("menuToolsSelectRegion")));
	addToolItem(new ToolButton(listener, gui, "SELECT_RECTANGLE", ACTION_TOOL_SELECT_RECT, GROUP_TOOL, true,
							   "rect-select.svg", _C("Select Rectangle"), gui->get("menuToolsSelectRectangle")));
	addToolItem(new ToolButton(listener, gui, "SELECT_OBJECT", ACTION_TOOL_SELECT_OBJECT, GROUP_TOOL, true,
							   "object-select.svg", _C("Select Object"), gui->get("menuToolsSelectObject")));

	addToolItem(new ToolButton(listener, gui, "DRAW_CIRCLE", ACTION_TOOL_DRAW_CIRCLE, GROUP_RULER, false,
							   "circle-draw.svg", _C("Draw Circle"), gui->get("menuToolsDrawCircle")));
	addToolItem(new ToolButton(listener, gui, "DRAW_RECTANGLE", ACTION_TOOL_DRAW_RECT, GROUP_RULER, false,
							   "rect-draw.svg", _C("Draw Rectangle"), gui->get("menuToolsDrawRect")));
	addToolItem(new ToolButton(listener, gui, "DRAW_ARROW", ACTION_TOOL_DRAW_ARROW, GROUP_RULER, false,
							   "arrow-draw.svg", _C("Draw Arrow"), gui->get("menuToolsDrawArrow")));

	addToolItem(new ToolButton(listener, gui, "VERTICAL_SPACE", ACTION_TOOL_VERTICAL_SPACE, GROUP_TOOL, true,
							   "stretch.svg", _C("Vertical Space"), gui->get("menuToolsVerticalSpace")));
	addToolItem(new ToolButton(listener, gui, "HAND", ACTION_TOOL_HAND, GROUP_TOOL, true, "hand.svg", _C("Hand"),
							   gui->get("menuToolsHand")));

	addToolItem(new ToolButton(listener, gui, "SHAPE_RECOGNIZER", ACTION_SHAPE_RECOGNIZER, GROUP_SHAPE_RECOGNIZER, false,
							   "shape_recognizer.svg", _C("Shape Recognizer"), gui->get("menuToolsShapeRecognizer")));
	addToolItem(new ToolButton(listener, gui, "RULER", ACTION_RULER, GROUP_RULER, false,
							   "ruler.svg", _C("Ruler"), gui->get("menuToolsRuler")));

	addToolItem(new ToolButton(listener, gui, "DOTTED", ACTION_DOTTED, GROUP_DOTTED, false,
								"tool_image.svg", _C("Dotted"), gui->get("menuToolsDotted")));

	addToolItem(new ToolButton(listener, gui, "FINE", ACTION_SIZE_FINE, GROUP_SIZE, true,
							   "thickness_thin.svg", _C("Thin")));
	addToolItem(new ToolButton(listener, gui, "MEDIUM", ACTION_SIZE_MEDIUM, GROUP_SIZE, true,
							   "thickness_medium.svg", _C("Medium")));
	addToolItem(new ToolButton(listener, gui, "THICK", ACTION_SIZE_THICK, GROUP_SIZE, true,
							   "thickness_thick.svg", _C("Thick")));

	addToolItem(new ToolButton(listener, gui, "DEFAULT_TOOL", ACTION_TOOL_DEFAULT, GROUP_NOGROUP, false,
							   "default.svg", _C("Default Tool"), gui->get("menuToolsDefault")));

	fontButton = new FontButton(listener, gui, "SELECT_FONT", ACTION_FONT_BUTTON_CHANGED, _C("Select Font"));
	addToolItem(fontButton);

	// Footer tools
	toolPageSpinner = new ToolPageSpinner(gui, listener, "PAGE_SPIN", ACTION_FOOTER_PAGESPIN);
	addToolItem(toolPageSpinner);

	ToolZoomSlider* toolZoomSlider = new ToolZoomSlider(listener, "ZOOM_SLIDER", ACTION_FOOTER_ZOOM_SLIDER, zoom);
	addToolItem(toolZoomSlider);

	addToolItem(new ToolButton(listener, gui, "TWO_PAGES", ACTION_VIEW_TWO_PAGES, GROUP_TWOPAGES, false,
							   "showtwopages.svg", _C("Two pages"), gui->get("menuViewTwoPages")));

	addToolItem(new ToolButton(listener, gui, "PRESENTATION_MODE", ACTION_VIEW_PRESENTATION_MODE, GROUP_PRESENTATION_MODE, false,
							   "showtwopages.svg", _C("Presentation mode"), gui->get("menuViewPresMode")));

	toolPageLayer = new ToolPageLayer(gui, listener, "LAYER", ACTION_FOOTER_LAYER);
	addToolItem(toolPageLayer);

	registerMenupoint(gui->get("menuEditSettings"), ACTION_SETTINGS);
	registerMenupoint(gui->get("menuFileAnnotate"), ACTION_ANNOTATE_PDF);

	registerMenupoint(gui->get("menuFileSaveAs"), ACTION_SAVE_AS);
	registerMenupoint(gui->get("menuFileExportPdf"), ACTION_EXPORT_AS_PDF);
	registerMenupoint(gui->get("menuFileExportAs"), ACTION_EXPORT_AS);

	registerMenupoint(gui->get("menuDocumentProperties"), ACTION_DOCUMENT_PROPERTIES);
	registerMenupoint(gui->get("menuFilePrint"), ACTION_PRINT);

	registerMenupoint(gui->get("menuFileQuit"), ACTION_QUIT);

	registerMenupoint(gui->get("menuJournalNewLayer"), ACTION_NEW_LAYER);
	registerMenupoint(gui->get("menuJournalDeleteLayer"), ACTION_DELETE_LAYER);
	registerMenupoint(gui->get("menuJournalNewPageBefore"), ACTION_NEW_PAGE_BEFORE);
	registerMenupoint(gui->get("menuJournalNewPageAfter"), ACTION_NEW_PAGE_AFTER);
	registerMenupoint(gui->get("menuJournalNewPageAtEnd"), ACTION_NEW_PAGE_AT_END);
	registerMenupoint(gui->get("menuDeletePage"), ACTION_DELETE_PAGE);
	registerMenupoint(gui->get("menuJournalPaperFormat"), ACTION_PAPER_FORMAT);
	registerMenupoint(gui->get("menuJournalPaperColor"), ACTION_PAPER_BACKGROUND_COLOR);

	registerMenupoint(gui->get("menuJournalPaperPlain"), ACTION_SET_PAPER_BACKGROUND_PLAIN);
	registerMenupoint(gui->get("menuJournalPaperBackgroundLined"), ACTION_SET_PAPER_BACKGROUND_LINED);
	registerMenupoint(gui->get("menuJournalPaperRuled"), ACTION_SET_PAPER_BACKGROUND_RULED);
	registerMenupoint(gui->get("menuJournalPaperGraph"), ACTION_SET_PAPER_BACKGROUND_GRAPH);
	registerMenupoint(gui->get("menuJournalPaperImage"), ACTION_SET_PAPER_BACKGROUND_IMAGE);
	registerMenupoint(gui->get("menuJournalPaperPdf"), ACTION_SET_PAPER_BACKGROUND_PDF);

	registerMenupoint(gui->get("menuEditDelete"), ACTION_DELETE);

	registerMenupoint(gui->get("menuNavigationPreviousAnnotatedPage"), ACTION_GOTO_PREVIOUS_ANNOTATED_PAGE);

	registerMenupoint(gui->get("eraserFine"), ACTION_TOOL_ERASER_SIZE_FINE, GROUP_ERASER_SIZE);
	registerMenupoint(gui->get("eraserMedium"), ACTION_TOOL_ERASER_SIZE_MEDIUM, GROUP_ERASER_SIZE);
	registerMenupoint(gui->get("eraserThick"), ACTION_TOOL_ERASER_SIZE_THICK, GROUP_ERASER_SIZE);

	registerMenupoint(gui->get("penthicknessVeryFine"), ACTION_TOOL_PEN_SIZE_VERY_THIN, GROUP_PEN_SIZE);
	registerMenupoint(gui->get("penthicknessFine"), ACTION_TOOL_PEN_SIZE_FINE, GROUP_PEN_SIZE);
	registerMenupoint(gui->get("penthicknessMedium"), ACTION_TOOL_PEN_SIZE_MEDIUM, GROUP_PEN_SIZE);
	registerMenupoint(gui->get("penthicknessThick"), ACTION_TOOL_PEN_SIZE_THICK, GROUP_PEN_SIZE);
	registerMenupoint(gui->get("penthicknessVeryThick"), ACTION_TOOL_PEN_SIZE_VERY_THICK, GROUP_PEN_SIZE);

	registerMenupoint(gui->get("highlighterFine"), ACTION_TOOL_HILIGHTER_SIZE_FINE, GROUP_HILIGHTER_SIZE);
	registerMenupoint(gui->get("highlighterMedium"), ACTION_TOOL_HILIGHTER_SIZE_MEDIUM, GROUP_HILIGHTER_SIZE);
	registerMenupoint(gui->get("highlighterThick"), ACTION_TOOL_HILIGHTER_SIZE_THICK, GROUP_HILIGHTER_SIZE);

	registerMenupoint(gui->get("menuToolsTextFont"), ACTION_SELECT_FONT);

#ifdef ENABLE_MATHTEX
	registerMenupoint(gui->get("menuEditTex"), ACTION_TEX);
#endif

	registerMenupoint(gui->get("menuViewToolbarManage"), ACTION_MANAGE_TOOLBAR);
	registerMenupoint(gui->get("menuViewToolbarCustomize"), ACTION_CUSTOMIZE_TOOLBAR);

	// Menu Help
	registerMenupoint(gui->get("menuHelpAbout"), ACTION_ABOUT);
	registerMenupoint(gui->get("menuHelpHelp"), ACTION_HELP);
}

void ToolMenuHandler::setFontButtonFont(XojFont& font)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	this->fontButton->setFont(font);
}

XojFont ToolMenuHandler::getFontButtonFont()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	return this->fontButton->getFont();
}

void ToolMenuHandler::showFontSelectionDlg()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	this->fontButton->showFontDialog();
}

void ToolMenuHandler::setUndoDescription(string description)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	this->undoButton->updateDescription(description);
	gtk_menu_item_set_label(GTK_MENU_ITEM(gui->get("menuEditUndo")), description.c_str());
}

void ToolMenuHandler::setRedoDescription(string description)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	this->redoButton->updateDescription(description);
	gtk_menu_item_set_label(GTK_MENU_ITEM(gui->get("menuEditRedo")), description.c_str());
}

SpinPageAdapter* ToolMenuHandler::getPageSpinner()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	return this->toolPageSpinner->getPageSpinner();
}

void ToolMenuHandler::setPageText(string text)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	this->toolPageSpinner->setText(text);
}

int ToolMenuHandler::getSelectedLayer()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	return this->toolPageLayer->getSelectedLayer();
}

void ToolMenuHandler::setLayerCount(int count, int selected)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	this->toolPageLayer->setLayerCount(count, selected);
}

ToolbarModel* ToolMenuHandler::getModel()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	return this->tbModel;
}

bool ToolMenuHandler::isColorInUse(int color)
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	for (ColorToolItem* it : this->toolbarColorItems)
	{
		if (it->getColor() == color)
		{
			return true;
		}
	}

	return false;
}

AbstractToolItemVector* ToolMenuHandler::getToolItems()
{
	XOJ_CHECK_TYPE(ToolMenuHandler);

	return &this->toolItems;
}
