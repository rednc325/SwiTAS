#pragma once

extern "C" {
#include <string.h>
#include <switch.h>
#include <stdlib.h>
}

#include "lvgl/lvgl.h"

#include "decodeAndResize.hpp"
#include "ui.hpp"
#include "writeToScreen.hpp"

class UI() {
	private:
	// 0x32000 is the size of the video buffer
	// I'm not quite sure why it is that, but it is
	u8* Vbuf
	Service grcdVideo;
	// Dimensions of the screen
	constexpr int width = 1280;
	constexpr int height = 720;
	// Dimensions of the game window in the corner
	constexpr int gameWidth = 960;
	constexpr int gameHeight = 540;
	// Current display instance
	ViDisplay disp;
	// VSync event of display
	Event vsyncEvent;
	// Decoding and resizing instance
	DecodeAndResize decodeAndResize;
	// RGBA buffers
	u8* inputRgbaBuffer;
	// RGBA buffer of game window in corner
	u8* gameImage;
	// This is returned by the other class
	//u8* outputRgbaBuffer = malloc(gameWidth * gameHeight * 4);
	// Debug handle (for pausing)
	Handle debugHandle;
	// Wether the game is currently disableGameRendering
	constexpr bool gameIsRendering = true;
	// Global result variable for all functions
	Result rc;
	// UI overlay instance
	AppUI* appUI;
	// WriteToScreen instance
	WriteToScreen* writeToScreen;
	// wether the UI is open
	constexpr bool UIOpen = false;


// Borrowed from sysDVR
Result grcdServiceOpen(Service* out) {
    if (serviceIsActive(out))
        return 0;

    rc = smGetService(out, "grc:d");

    if (R_FAILED(rc)) grcdExit();

    return rc;
}

// Borrowed from sysDVR
void grcdServiceClose(Service* svc){
	serviceClose(svc);
}

// Borrowed from sysDVR
Result grcdServiceBegin(Service* svc){
    return _grcCmdNoIO(svc, 1);
}

// Borrowed from sysDVR
Result grcdServiceRead(Service* svc, GrcStream stream, void* buffer, size_t size, u32 *unk, u32 *data_size, u64 *timestamp) {
    IpcCommand c;
    ipcInitialize(&c);

    ipcAddRecvBuffer(&c, buffer, size, BufferType_Normal);

    struct {
        u64 magic;
        u64 cmd_id;
        u32 stream;
    } *raw;

    raw = serviceIpcPrepareHeader(svc, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 2;
    raw->stream = stream;

    rc = serviceIpcDispatch(svc);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        struct {
            u64 magic;
            u64 result;
            u32 unk;
            u32 data_size;
            u64 timestamp;
        } *resp;

        serviceIpcParse(svc, &r, sizeof(*resp));
        resp = r.Raw;

        rc = resp->result;

        if (R_SUCCEEDED(rc)) {
            if (unk) *unk = resp->unk;
            if (data_size) *data_size = resp->data_size;
            if (timestamp) *timestamp = resp->timestamp;
        }
    }

    return rc;
}

// Borrowed from sysDVR
Result _grcCmdNoIO(Service* srv, u64 cmd_id) {
	IpcCommand c;
	ipcInitialize(&c);

	struct {
		u64 magic;
		u64 cmd_id;
	} *raw;

	raw = serviceIpcPrepareHeader(srv, &c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = cmd_id;

	rc = serviceIpcDispatch(srv);

	if (R_SUCCEEDED(rc)) {
		IpcParsedCommand r;
		struct {
			u64 magic;
			u64 result;
		} *resp;

		serviceIpcParse(srv, &r, sizeof(*resp));
		resp = r.Raw;

		rc = resp->result;
	}

	return rc;
}

	public:
	NxTASUI() {
		// Create neccessary instances
		writeToScreen = new WriteToScreen(width, height);
		// All the code neccessary to make sure the L UI is right
		appUI = new AppUI(width - gameWidth, height,
			gameWidth, height - gameHeight, writeToScreen);
		// Create buffers
		inputRgbaBuffer = malloc(width * height * 4);
		Vbuf = aligned_alloc(0x1000, 0x32000);
		// Open video service if it is not already open
		rc = grcdServiceOpen(&grcdVideo);
		if (R_FAILED(rc)) fatalSimple(rc);
		rc = grcdServiceBegin(&grcdVideo);
		if (R_FAILED(rc)) fatalSimple(rc);
		// Create decoding and resizing instance
		decodeAndResize = new DecodeAndResize(width, heght, gameWidth, gameHeight);
		// Create display and vsync instances
    	rc = viOpenDefaultDisplay(&disp);
    	if(R_FAILED(rc)) fatalSimple(rc);
    	rc = viGetDisplayVsyncEvent(&disp, &vsyncEvent);
    	if(R_FAILED(rc)) fatalSimple(rc);
	}

	void enableGameRendering() {
		// I believe it closes the debug process
		if (!gameIsRendering) {
			svcCloseHandle(debugHandle);
			gameIsRendering = true;
		}
	}

	void disableGameRendering() {
		if (gameIsRendering) {
			u64 pid = 0;
			// Get the PID of the currently running game, I believe...
        	pmdmntGetApplicationPid(&pid);
			// Pauses the game through debugging
			// Apparently applications pause when they are debugging, so I am using this to my advantage
    		svcDebugActiveProcess(&debugHandle, pid);
			gameIsRendering = false;
		}
	}

	void drawGameWindowAndUi() {
		// Make sure game is rendering
		enableGameRendering();
		// This function produces H264 frames in Vbuf with a size of 1280 x 720
		// I'm not exactly sure what this is, but it's always 0
		u32 unk = 0;
		u32 VOutSz = 0;
		u64 timestamp = 0;
		// GrcStream_Video is a constant from LibNX
		// 0x32000 is VBufZ, size of the video buffer
		int fails = 0;
		// Runs multiple times to make sure it works
		// From sysDVR
		/*
		while (true) {
			Result res = grcdServiceRead(&grcdVideo, GrcStream_Video, Vbuf, 0x32000, &unk, &VOutSz, &timestamp)
			if (R_SUCCEEDED(res) && VOutSz > 0) break;
			VOutSz = 0;
			if (fails++ > 8 && !IsThreadRunning) break;
		}
		*/
		// Wait for this vsync to get a single frame
		rc = eventWait(&vsync_event, 0xFFFFFFFFFFF);
        if(R_FAILED(rc)) fatalSimple(rc);
		rc = grcdServiceRead(&grcdVideo, GrcStream_Video, Vbuf, 0x32000, &unk, &VOutSz, &timestamp);
		if(R_FAILED(rc)) fatalSimple(rc);
		// Stop rendering now, before another frame is written
		disableGameRendering();
		// Decode and resizing the H264 frame with the other function
		gameImage = decodeAndResize.decodeAndResize(Vbuf, 0x32000);
		// gameImage is an RGBA array
		writeToScreen->startFrame();
		for (int x = 0; x < gameWidth; x++) {
			for (int y = 0; y < gameHeight; y++) {
				// Assign the pixel now
				int pixelOffset = y * gameWidth * 4 + x * 4;
				writeToScreen->setPixel(x + AppUI->getLeftWidth(), y,
					gameImage[pixelOffset],     // R
					gameImage[pixelOffset + 1], // G
					gameImage[pixelOffset + 2]);// B
			}
		}
		// Deal with LVGL UI
		for (int x = 0; x < AppUI->getLeftWidth(); X++) {
			for (int y = 0; y < appUI->getLeftHeight(); y++) {
				int pixelOffset = y * AppUI->getLeftWidth() + x;
				lv_color_t pixelColor = AppUI->getLeftBuf()[pixelOffset];
				writeToScreen->setPixel(x, y,
					pixelColor.red,
					pixelColor.green,
					pixelColor.blue);
			}
		}
		for (int x = 0; x < AppUI->getBottomWidth(); X++) {
			for (int y = 0; y < appUI->getBottomHeight(); y++) {
				int pixelOffset = y * AppUI->getBottomWidth() + x;
				lv_color_t pixelColor = AppUI->getBottomBuf()[pixelOffset];
				writeToScreen->setPixel(x + AppUI->getLeftWidth(),
					y + gameHeight,
					pixelColor.red,
					pixelColor.green,
					pixelColor.blue);
			}
		}
		// Finally, write to framebuffer
		writeToScreen->endFrame();
		// UI and game window are now successfully written to screen
	}

	void openUI() {
		// The good stuff where everything happens
		UIOpen = true
		disableGameRendering();
		appUI->enableDrawing();
		// Do the UI thing as much as you can
		while (UIOpen) {
			// Draw the game window in the corner and leave rendering open for the other UI
			drawGameWindowAndUi();
			// Some condition to close the UI by setting UIOpen to false
			if (!UIOpen) {
				closeUI();
			}
		}
	}

	void closeUI() {
		// Called from openUI in the while loop
		appUI->enableDrawing();
		enableGameRendering();
	}

	~NxTASUI() {
		// Stop service
		grcdServiceClose(&grcdVideo);
		// Free the video buffers, both H264 and raw RGBA
		free(Vbuf);
		free(inputRgbaBuffer);
		free(outputRgbaBuffer);
		// The decoding and resizing instance will automatically be deleted
	}
}