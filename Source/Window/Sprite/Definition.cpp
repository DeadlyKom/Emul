#include "Definition.h"

bool Window::ClipboardData(FRGBAImage& OutputRGBAImage)
{
	ON_SCOPE_EXIT
	{
		CloseClipboard();
	};

	if (!OpenClipboard(nullptr))
	{
		return false;
	}

	HBITMAP hBM = nullptr;
	if (IsClipboardFormatAvailable(CF_BITMAP))
	{
		hBM = (HBITMAP)GetClipboardData(CF_BITMAP);
	}
	else if (IsClipboardFormatAvailable(CF_DIB))
	{
		hBM = (HBITMAP)GetClipboardData(CF_DIB);
	}

	if (!hBM)
	{
		return false;
	}


	BITMAP BM{};
	GetObject(hBM, sizeof(BITMAP), &BM);

	OutputRGBAImage.Width = (int32_t)BM.bmWidth;
	OutputRGBAImage.Height = (int32_t)BM.bmHeight;
	OutputRGBAImage.Data.resize(BM.bmWidth * BM.bmHeight * 4);

	BITMAPINFOHEADER bi{};
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = BM.bmWidth;
	bi.biHeight = -BM.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;

	HDC hdc = GetDC(nullptr);
	if (!GetDIBits(hdc, hBM, 0, BM.bmHeight, OutputRGBAImage.Data.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS))
	{
		ReleaseDC(nullptr, hdc);
		return false;
	}
	ReleaseDC(nullptr, hdc);

	for (size_t i = 0; i < OutputRGBAImage.Data.size(); i += 4)
	{
		std::swap(OutputRGBAImage.Data[i + 0], OutputRGBAImage.Data[i + 2]); // B <-> R
		OutputRGBAImage.Data[i + 3] = 255; // Alpha = 255
	}

	return true;
}
