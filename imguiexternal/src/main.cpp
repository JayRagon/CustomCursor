#include <iostream>
#include <Windows.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <thread>
#include <filesystem>
#include <fstream>

using namespace std;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

filesystem::path filePath = "C:\\_Rage\\_DBGCURSOR.xor";

LONG TrailLength = 50;
double TrailDecay = 0.99;

POINT TrailPoint[3000] = {};
ImColor TrailColor[3000] = {};
float TrailSize[3000] = {};
POINT GradPoint[30] = {};
ImColor GradColor[30] = {};
float GradSize[30] = {};
LONG TrailCounter = 0x1;

HDC hdc = GetDC(NULL);

int SW = GetDeviceCaps(hdc, HORZRES);
int SH = GetDeviceCaps(hdc, VERTRES);

float HUE = 0;
float S = 1;
float V = 1;

/*! \brief Convert HSV to RGB color space

  Converts a given set of HSV values `h', `s', `v' into RGB
  coordinates. The output RGB values are in the range [0, 1], and
  the input HSV values are in the ranges h = [0, 360], and s, v =
  [0, 1], respectively.

  \param fR Red component, used as output, range: [0, 1]
  \param fG Green component, used as output, range: [0, 1]
  \param fB Blue component, used as output, range: [0, 1]
  \param fH Hue component, used as input, range: [0, 360]
  \param fS Hue component, used as input, range: [0, 1]
  \param fV Hue component, used as input, range: [0, 1]

*/
void HSVtoRGB(float& fR, float& fG, float& fB, float& fH, float& fS, float& fV) {
	float fC = fV * fS; // Chroma
	float fHPrime = fmod(fH / 60.0, 6);
	float fX = fC * (1 - fabs(fmod(fHPrime, 2) - 1));
	float fM = fV - fC;

	if (0 <= fHPrime && fHPrime < 1) {
		fR = fC;
		fG = fX;
		fB = 0;
	}
	else if (1 <= fHPrime && fHPrime < 2) {
		fR = fX;
		fG = fC;
		fB = 0;
	}
	else if (2 <= fHPrime && fHPrime < 3) {
		fR = 0;
		fG = fC;
		fB = fX;
	}
	else if (3 <= fHPrime && fHPrime < 4) {
		fR = 0;
		fG = fX;
		fB = fC;
	}
	else if (4 <= fHPrime && fHPrime < 5) {
		fR = fX;
		fG = 0;
		fB = fC;
	}
	else if (5 <= fHPrime && fHPrime < 6) {
		fR = fC;
		fG = 0;
		fB = fX;
	}
	else {
		fR = 0;
		fG = 0;
		fB = 0;
	}

	fR += fM;
	fG += fM;
	fB += fM;
}


LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param))
	{
		return 0L;
	}

	if (message == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0L;
	}

	return DefWindowProc(window, message, w_param, l_param);
}

INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show)
{
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = instance;
	wc.lpszClassName = L"External Overay Class";

	RegisterClassExW(&wc);

	const HWND window = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
		wc.lpszClassName,
		L"External Overlay",
		WS_POPUP,
		0,
		0,
		SW,
		SH,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	SetLayeredWindowAttributes(window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

	RECT client_area{};
	GetClientRect(window, &client_area);

	RECT window_area{};
	GetWindowRect(window, &window_area);

	POINT diff{};
	ClientToScreen(window, &diff);

	const MARGINS margins{
		window_area.left + (diff.x - window_area.left),
		window_area.top + (diff.y - window_area.top),
		client_area.right,
		client_area.bottom
	};

	DwmExtendFrameIntoClientArea(window, &margins);
	
	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.RefreshRate.Numerator = 144U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = window;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	constexpr D3D_FEATURE_LEVEL levels[2]
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};

	ID3D11Device* device{ nullptr };
	ID3D11DeviceContext* device_context{ nullptr };
	IDXGISwapChain* swap_chain{ nullptr };
	ID3D11RenderTargetView* render_target_view{ nullptr };
	D3D_FEATURE_LEVEL level{};

	D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		levels,
		2U,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&level,
		&device_context
	);

	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer)
	{
		device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
		back_buffer->Release();
	}
	else
	{
		return 1;
	}

	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(device, device_context);

	bool running = true;

	ofstream outfile(filePath);
	while (running)
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				running = false;
			}
		}

		if (!running)
		{
			break;
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();

		ImGui::NewFrame();

		//ImColor(1.f, 1.f, 1.f)   AddCircleFilled({ 500, 100 }, 100.f, ImColor(1.f, 1.f, 1.f));

		float SWC = SW / 2;
		float SHC = SH / 2;

		//SWC - (FOVX / 2), SHC - (FOVY / 2)

		// shift hue

		HUE += 0.5;

		if (HUE > 360)
		{
			HUE = 0;
		}

		float R = 0;
		float G = 0;
		float B = 0;

		HSVtoRGB(R, G, B, HUE, S, V);

		POINT p;
		if (GetCursorPos(&p))
		{
			if (TrailCounter == TrailLength)
			{
				// Shift the trail points to make space for the new point
				for (DWORD i = 1; i < TrailLength; i++)
				{
					TrailPoint[i - 1] = TrailPoint[i];
					TrailSize[i - 1] = TrailSize[i];
					TrailColor[i - 1] = TrailColor[i];
				}

				TrailCounter = TrailLength - 1;
			}

			// Update the last trail point
			TrailPoint[TrailCounter] = p;
			TrailColor[TrailCounter] = { R, G, B };
			TrailSize[TrailCounter] = 20.f;

			// Draw the trail
			for (DWORD i = 1; i < TrailCounter; i++)
			{
				TrailSize[i] *= TrailDecay;

				ImGui::GetBackgroundDrawList()->AddLine(
					{ (float)TrailPoint[i - 1].x, (float)TrailPoint[i - 1].y },
					{ (float)TrailPoint[i].x, (float)TrailPoint[i].y },
					TrailColor[i], TrailSize[i] * 2);

				ImGui::GetBackgroundDrawList()->AddCircleFilled(
					{ (float)TrailPoint[i].x, (float)TrailPoint[i].y },
					TrailSize[i], TrailColor[i]);
			}

			// Draw the static circle
			ImGui::GetBackgroundDrawList()->AddCircleFilled(
				{ (float)p.x, (float)p.y }, 14.f, ImColor(1.f, 1.f, 1.f));

			TrailCounter++;
		}
		else
		{
			return 300;
		}

		ImGui::EndFrame();
		ImGui::Render();

		constexpr float color[4]{ 0.f, 0.f, 0.f, 0.f };
		device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);
		device_context->ClearRenderTargetView(render_target_view, color);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		
		//this_thread::sleep_for(chrono::microseconds(50));

		swap_chain->Present(1U, 0U);
	}

	outfile.close();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	if (swap_chain){
		swap_chain->Release();
	}
	if (device_context) {
		device_context->Release();
	}
	if (device) {
		device->Release();
	}
	if (render_target_view) {
		render_target_view->Release();
	}

	DestroyWindow(window);

	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}