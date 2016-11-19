// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <windows.h>

#include "Common/GL/GLInterface/WGL.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

static HDC hDC = nullptr;              // Private GDI Device Context
static HGLRC hRC = nullptr;            // Permanent Rendering Context
static HINSTANCE dllHandle = nullptr;  // Handle to OpenGL32.dll

// typedef from wglext.h
typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;

void cInterfaceWGL::SwapInterval(int Interval)
{
	if (wglSwapIntervalEXT)
		wglSwapIntervalEXT(Interval);
	else
		ERROR_LOG(VIDEO, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
}
void cInterfaceWGL::Swap()
{
	SwapBuffers(hDC);
}

void* cInterfaceWGL::GetFuncAddress(const std::string& name)
{
	void* func = (void*)wglGetProcAddress((LPCSTR)name.c_str());
	if (func == nullptr)
		func = (void*)GetProcAddress(dllHandle, (LPCSTR)name.c_str());
	return func;
}

// Draw messages on top of the screen
bool cInterfaceWGL::PeekMessages()
{
	// TODO: peekmessage
	MSG msg;
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return FALSE;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return TRUE;
}

#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB             0x2093
#define WGL_CONTEXT_FLAGS_ARB                   0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126

#define WGL_CONTEXT_DEBUG_BIT_ARB               0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002

#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

static HGLRC wglCreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int *attribList)
{
typedef HGLRC(APIENTRY * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);
	static PFNWGLCREATECONTEXTATTRIBSARBPROC pfnCreateContextAttribsARB = 0;

	HGLRC hContext = 0;
	HGLRC hCurrentContext = 0;
	
	if (!(hCurrentContext = wglCreateContext(hDC)))
		return 0;

	if (wglMakeCurrent(hDC, hCurrentContext))
	{
		if (!pfnCreateContextAttribsARB)
			pfnCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));

		if (pfnCreateContextAttribsARB)
		{
			hContext = pfnCreateContextAttribsARB(hDC, hShareContext, attribList);
		}
	}
	wglDeleteContext(hCurrentContext);
	
	return hContext;
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceWGL::Create(void* window_handle, bool core)
{
	if (window_handle == nullptr)
		return false;

	HWND window_handle_reified = reinterpret_cast<HWND>(window_handle);
	RECT window_rect = { 0 };

	if (!GetClientRect(window_handle_reified, &window_rect))
		return false;

	// Control window size and picture scaling
	int twidth = (window_rect.right - window_rect.left);
	int theight = (window_rect.bottom - window_rect.top);
	s_backbuffer_width = twidth;
	s_backbuffer_height = theight;

	m_window_handle = window_handle_reified;

	dllHandle = LoadLibrary(TEXT("OpenGL32.dll"));

	PIXELFORMATDESCRIPTOR pfd =  // pfd Tells Windows How We Want Things To Be
	{
			sizeof(PIXELFORMATDESCRIPTOR),  // Size Of This Pixel Format Descriptor
			1,                              // Version Number
			PFD_DRAW_TO_WINDOW |            // Format Must Support Window
					PFD_SUPPORT_OPENGL |        // Format Must Support OpenGL
					PFD_DOUBLEBUFFER,           // Must Support Double Buffering
			PFD_TYPE_RGBA,                  // Request An RGBA Format
			32,                             // Select Our Color Depth
			0,
			0, 0, 0, 0, 0,   // Color Bits Ignored
			0,               // 8bit Alpha Buffer
			0,               // Shift Bit Ignored
			0,               // No Accumulation Buffer
			0, 0, 0, 0,      // Accumulation Bits Ignored
			0,               // 0Bit Z-Buffer (Depth Buffer)
			0,               // 0bit Stencil Buffer
			0,               // No Auxiliary Buffer
			PFD_MAIN_PLANE,  // Main Drawing Layer
			0,               // Reserved
			0, 0, 0          // Layer Masks Ignored
	};

	int PixelFormat;  // Holds The Results After Searching For A Match

	if (!(hDC = GetDC(window_handle_reified)))
	{
		PanicAlert("(1) Can't create an OpenGL Device context. Fail.");
		return false;
	}

	if (!(PixelFormat = ChoosePixelFormat(hDC, &pfd)))
	{
		PanicAlert("(2) Can't find a suitable PixelFormat.");
		return false;
	}

	if (!SetPixelFormat(hDC, PixelFormat, &pfd))
	{
		PanicAlert("(3) Can't set the PixelFormat.");
		return false;
	}

	std::array<std::pair<int, int>, 7> versions_to_try = { {
		{ 4, 5 },{ 4, 4 },{ 4, 3 },{ 4, 2 },{ 4, 1 },{ 4, 0 },{ 3, 3 },
		} };

	for (const auto& version : versions_to_try)
	{
		int attriblist[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, version.first,
			WGL_CONTEXT_MINOR_VERSION_ARB, version.second,
			WGL_CONTEXT_FLAGS_ARB, 0,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0, 0 };

		hRC = wglCreateContextAttribsARB(hDC, 0, attriblist);
		if (hRC)
			break;
	}
	if (!hRC)
	{
		if (!(hRC = wglCreateContext(hDC)))
		{
			PanicAlert("(4) Can't create an OpenGL rendering context.");
			return false;
		}
	}
	

	return true;
}

bool cInterfaceWGL::MakeCurrent()
{
	bool success = wglMakeCurrent(hDC, hRC) ? true : false;
	if (success)
	{
		// Grab the swap interval function pointer
		wglSwapIntervalEXT =
			(PFNWGLSWAPINTERVALEXTPROC)GLInterface->GetFuncAddress("wglSwapIntervalEXT");
	}
	return success;
}

bool cInterfaceWGL::ClearCurrent()
{
	return wglMakeCurrent(hDC, nullptr) ? true : false;
}

// Update window width, size and etc. Called from Render.cpp
void cInterfaceWGL::Update()
{
	RECT rcWindow;
	GetClientRect(m_window_handle, &rcWindow);

	// Get the new window width and height
	s_backbuffer_width = (rcWindow.right - rcWindow.left);
	s_backbuffer_height = (rcWindow.bottom - rcWindow.top);

	// Get the top-left corner of the client area in screen coordinates
	POINT originPoint = { 0, 0 };
	ClientToScreen(m_window_handle, &originPoint);
	s_window_xpos = originPoint.x;
	s_window_ypos = originPoint.y;
}

// Close backend
void cInterfaceWGL::Shutdown()
{
	if (hRC)
	{
		if (!wglMakeCurrent(nullptr, nullptr))
			NOTICE_LOG(VIDEO, "Could not release drawing context.");

		if (!wglDeleteContext(hRC))
			ERROR_LOG(VIDEO, "Attempt to release rendering context failed.");

		hRC = nullptr;
	}

	if (hDC && !ReleaseDC(m_window_handle, hDC))
	{
		ERROR_LOG(VIDEO, "Attempt to release device context failed.");
		hDC = nullptr;
	}
	FreeLibrary(dllHandle);
}
