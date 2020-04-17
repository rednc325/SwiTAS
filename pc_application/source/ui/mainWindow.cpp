#define RAPIDJSON_HAS_STDSTRING 1

#include "mainWindow.hpp"

MainWindow::MainWindow()
	: wxFrame(NULL, wxID_ANY, "NX TAS UI", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxMAXIMIZE) {
	wxImage::AddHandler(new wxPNGHandler());
	wxImage::AddHandler(new wxJPEGHandler());

	// OwO what dis?
	SetDoubleBuffered(true);

	// Initialize FFMS2 because it can only happen once
	FFMS_Init(0, 0);

	// Get the main settings
	mainSettings = HELPERS::getSettingsFile("../mainSettings.json");

	wxIcon mainicon;
	mainicon.LoadFile(HELPERS::resolvePath(mainSettings["programIcon"].GetString()), wxBITMAP_TYPE_PNG);
	SetIcon(mainicon);

	// https://forums.wxwidgets.org/viewtopic.php?t=28894
	// https://cboard.cprogramming.com/cplusplus-programming/92653-starting-wxwidgets-wxpanel-full-size-frame.html
	// This means some things have to change going on
	mainSizer = new wxBoxSizer(wxHORIZONTAL);

	// Set button data instance
	buttonData = std::make_shared<ButtonData>();
	// Load button data here
	buttonData->setupButtonMapping(&mainSettings);

	// Start networking with set queues
	networkInstance = std::make_shared<CommunicateWithNetwork>(
		[](CommunicateWithNetwork* self) {
			SEND_QUEUE_DATA(SendFlag)
			SEND_QUEUE_DATA(SendRunFrame)
			SEND_QUEUE_DATA(SendLogging)
		},
		[](CommunicateWithNetwork* self) {
			RECIEVE_QUEUE_DATA(RecieveFlag)
			RECIEVE_QUEUE_DATA(RecieveGameInfo)
			RECIEVE_QUEUE_DATA(RecieveGameFramebuffer)
			RECIEVE_QUEUE_DATA(RecieveApplicationConnected)
			RECIEVE_QUEUE_DATA(RecieveLogging)
		});

	// DataProcessing can now start with the networking instance
	dataProcessingInstance = new DataProcessing(&mainSettings, buttonData, networkInstance, this);

	// UI instances
	sideUI   = std::make_shared<SideUI>(this, &mainSettings, mainSizer, dataProcessingInstance);
	bottomUI = std::make_shared<BottomUI>(this, &mainSettings, buttonData, mainSizer, dataProcessingInstance);

	// Add the top menubar and the bottom statusbar
	addStatusBar();
	addMenuBar();

	// No fit for now
	SetSizer(mainSizer);
	mainSizer->SetSizeHints(this);
	Layout();
	Fit();
	Center(wxBOTH);

#ifdef _WIN32
	// Enable dark mode, super experimential, apparently
	// needs to be applied to every window, however
	SetWindowTheme(GetHWND(), L"DarkMode_Explorer", NULL);
	Refresh();
#endif

	// Override the keypress handler
	// add_events(Gdk::KEY_PRESS_MASK);
	handlePreviousWindowTransform();

	projectHandler = std::make_shared<ProjectHandler>(dataProcessingInstance, &mainSettings);
}

void MainWindow::onStart() {
	logWindow = new wxLogWindow(this, "Logger", false, false);
	wxLog::SetTimestamp(wxS("%Y-%m-%d %H:%M: %S"));
	wxLog::SetActiveTarget(logWindow);

	debugWindow = new DebugWindow(networkInstance);

	ProjectHandlerWindow projectHandlerWindow(projectHandler, &mainSettings);

	projectHandlerWindow.ShowModal();

	if(projectHandlerWindow.wasDialogClosedForcefully()) {
		// It was closed with X, terminate this window, and the entire application, as well
		projectHandler->setProjectWasLoaded(false);
		Close(true);
		return;
	}

	if(!projectHandlerWindow.wasProjectChosen()) {
		// Generate a temp one
		projectHandlerWindow.createTempProjectDir();
	}
	Show();
}

// clang-format off
BEGIN_EVENT_TABLE(MainWindow, wxFrame)
	EVT_CHAR_HOOK(MainWindow::keyDownHandler)
	EVT_SIZE(MainWindow::OnSize)
	EVT_IDLE(MainWindow::onIdle)
	EVT_CLOSE(MainWindow::onClose)
END_EVENT_TABLE()
// clang-format on

// Override default signal handler:
void MainWindow::keyDownHandler(wxKeyEvent& event) {
	// Only handle keybard input if control is not held down
	if(!event.ControlDown()) {
		dataProcessingInstance->handleKeyboardInput(event.GetUnicodeKey());
	}

	// Always skip the event
	event.Skip();
}

void MainWindow::handlePreviousWindowTransform() {
	// Resize and maximize as needed
	// TODO
}

void MainWindow::OnSize(wxSizeEvent& event) {
	// https://wiki.wxwidgets.org/WxSizer#Sizer_Doesn.27t_Work_When_Making_a_Custom_Control.2FWindow_.28no_autolayout.29
	// https://forums.wxwidgets.org/viewtopic.php?t=28894
	if(GetAutoLayout()) {
		Layout();
	}
}

void MainWindow::onIdle(wxIdleEvent& event) {
	if(IsShown()) {
		// Listen to joystick
		bottomUI->listenToJoystick();
	}
}

void MainWindow::handleNetworkQueues() {
	// clang-format off
	CHECK_QUEUE(networkInstance, RecieveGameFramebuffer, {
		wxLogMessage("Framebuffer recieved");
		bottomUI->recieveGameFramebuffer(data.buf);
	})
	CHECK_QUEUE(networkInstance, RecieveApplicationConnected, {
		wxLogMessage("Game opened");
	})
	CHECK_QUEUE(networkInstance, RecieveLogging, {
		wxLogMessage(wxString("SWITCH: " + data.log));
	})
	// clang-format on
}

void MainWindow::addMenuBar() {
	menuBar = new wxMenuBar();

	wxMenu* fileMenu = new wxMenu();

	selectIPID                = NewControlId();
	setNameID                 = NewControlId();
	toggleLoggingID           = NewControlId();
	toggleDebugMenuID         = NewControlId();
	openVideoComparisonViewer = NewControlId();

	fileMenu->Append(selectIPID, "&Server");
	fileMenu->Append(setNameID, "&Set Name");

	// Add joystick submenu
	bottomUI->addJoystickMenu(fileMenu);

	fileMenu->Append(toggleLoggingID, "&Toggle Logging");
	fileMenu->Append(toggleDebugMenuID, "&Toggle Debug Menu");
	fileMenu->Append(openVideoComparisonViewer, "&Open Video Comparison Viewer");

	menuBar->Append(fileMenu, "&File");

	Bind(wxEVT_MENU, &MainWindow::handleMenuBar, this, wxID_ANY);

	SetMenuBar(menuBar);
}

void MainWindow::addStatusBar() {
	// 1 element for now
	CreateStatusBar(1);

	SetStatusText("No Network Connected", 0);
}

void MainWindow::handleMenuBar(wxCommandEvent& commandEvent) {
	wxWindowID id = commandEvent.GetId();
	if(id >= bottomUI->joystickSubmenuIDBase) {
		// Send straight to bottomUI
		bottomUI->onJoystickSelect(commandEvent);
	} else {
		// No switch statements for me
		if(id == selectIPID) {
			// IP needs to be selected
			wxString ipAddress = wxGetTextFromUser("Please enter IP address of Nintendo Switch", "Server connect", wxEmptyString);
			if(!ipAddress.empty()) {
				// IP address entered
				if(networkInstance->attemptConnectionToServer(ipAddress.ToStdString())) {
					SetStatusText(ipAddress + ":" + std::to_string(SERVER_PORT), 0);
				} else {
					wxMessageDialog addressInvalidDialog(this, wxString::Format("This IP address is invalid: %s", networkInstance->getLastErrorMessage().c_str()), "Invalid IP", wxOK);
					addressInvalidDialog.ShowModal();
				}
			}
		} else if(id == setNameID) {
			// Name needs to be selected
			wxString projectName = wxGetTextFromUser("Please set the new name of the project", "Set name", projectHandler->getProjectName());
			if(!projectName.empty()) {
				projectHandler->setProjectName(projectName.ToStdString());
			}
		} else if(id == toggleLoggingID) {
			logWindow->Show(!logWindow->GetFrame()->IsShown());
			wxLogMessage("Toggled log window");
		} else if(id == toggleDebugMenuID) {
			debugWindow->Show(!debugWindow->IsShown());
			wxLogMessage("Toggled debug window");
		} else if(id == openVideoComparisonViewer) {
			VideoComparisonViewer* viewer = new VideoComparisonViewer(&mainSettings, this, projectHandler->getProjectStart().GetPath());
			videoComparisonViewers.push_back(viewer);
			viewer->Show(true);
		}
	}
}

void MainWindow::onClose(wxCloseEvent& event) {
	// Close project dialog and save
	projectHandler->saveProject();

	delete wxLog::SetActiveTarget(NULL);

	// TODO, this raises errors for some reason
	// FFMS_Deinit();

	Destroy();
}