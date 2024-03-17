#include "RenderLab.h"

HWND mMainWnd = 0;
RenderLab* rl;
int mClientWidth = 800;
int mClientHeight = 600;

bool InitWindowsApp(HINSTANCE hInstance, int show);

int Run();

LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstanc, HINSTANCE hPrevInstance,
	PSTR pCmdLine, int nCmdShow)
{
	if (!InitWindowsApp(hInstanc, nCmdShow))
		return 0;

	rl = new RenderLab(mMainWnd, mClientHeight, mClientWidth);

	rl->Initialize();

	return Run();
}

bool InitWindowsApp(HINSTANCE hInstance, int show)
{
	WNDCLASS wc;

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"+++", 0, 0);
		return false;
	}

	mMainWnd = CreateWindow(
		L"MainWnd",
		L"D3DApp",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		mClientWidth,
		mClientHeight,
		0,
		0,
		hInstance,
		0
	);

	if (mMainWnd == 0)
	{
		MessageBox(0, L"---", 0, 0);
		return false;
	}

	ShowWindow(mMainWnd, show);
	UpdateWindow(mMainWnd);

	return true;
}

int Run()
{
	MSG msg = { 0 };

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			rl->Update();
			rl->Draw();
			

			/*if (!mAppPaused)
			{
				CalculateFrameStats();
				Update(mTimer);
				Draw(mTimer);
			}
			else
			{
				Sleep(100);
			}*/
		}
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_LBUTTONDOWN:
		MessageBox(0, L"===", L"***", MB_OK);
		return 0;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			DestroyWindow(mMainWnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}