#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <cwctype>
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <oleauto.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
using namespace std;
using Microsoft::WRL::ComPtr;
bool Enc, Dec;
wstring In, Out, Pwd = L"0";
int W, H, CW;
vector<unsigned char> Pix, Res;
vector<unsigned int> Cv;
unsigned long long Hash(const wstring& s) {
	unsigned long long h = 14695981039346656037ULL;
	for (wchar_t c : s) {
		h ^= (unsigned long long)(c & 0xFF); h *= 1099511628211ULL;
		h ^= (unsigned long long)((c >> 8) & 0xFF); h *= 1099511628211ULL;
	}
	return h;
}
inline int Sgn(int x) { return (x > 0) - (x < 0); }
inline int FD2(int x) { return x >= 0 ? x / 2 : (x - 1) / 2; }
inline void Psh(int x, int y) { Cv.push_back((unsigned int)y * (unsigned int)CW + (unsigned int)x); }
void Gen(int x, int y, int ax, int ay, int bx, int by) {
	int wd = abs(ax + ay), hg = abs(bx + by);
	int dax = Sgn(ax), day = Sgn(ay), dbx = Sgn(bx), dby = Sgn(by);
	if (hg == 1) { for (int i = 0; i < wd; i++, x += dax, y += day) Psh(x, y); return; }
	if (wd == 1) { for (int i = 0; i < hg; i++, x += dbx, y += dby) Psh(x, y); return; }
	int ax2 = FD2(ax), ay2 = FD2(ay), bx2 = FD2(bx), by2 = FD2(by);
	int w2 = abs(ax2 + ay2), h2 = abs(bx2 + by2);
	if (2 * wd > 3 * hg) {
		if ((w2 & 1) && (wd > 2)) ax2 += dax, ay2 += day;
		Gen(x, y, ax2, ay2, bx, by);
		Gen(x + ax2, y + ay2, ax - ax2, ay - ay2, bx, by);
	} else {
		if ((h2 & 1) && (hg > 2)) bx2 += dbx, by2 += dby;
		Gen(x, y, bx2, by2, ax2, ay2);
		Gen(x + bx2, y + by2, ax, ay, bx - bx2, by - by2);
		Gen(x + (ax - dax) + (bx2 - dbx), y + (ay - day) + (by2 - dby),
			-bx2, -by2, -(ax - ax2), -(ay - ay2));
	}
}
void Gilbert(int w, int h) {
	Cv.clear(), Cv.reserve((size_t)w * h), CW = w;
	if (w >= h) Gen(0, 0, w, 0, 0, h);
	else Gen(0, 0, 0, h, w, 0);
}
bool Load() {
	ComPtr<IWICImagingFactory> F;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&F)))) return 0;
	ComPtr<IWICBitmapDecoder> D;
	if (FAILED(F->CreateDecoderFromFilename(In.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &D))) return 0;
	ComPtr<IWICBitmapFrameDecode> Fr;
	if (FAILED(D->GetFrame(0, &Fr))) return 0;
	ComPtr<IWICFormatConverter> Conv;
	if (FAILED(F->CreateFormatConverter(&Conv))) return 0;
	if (FAILED(Conv->Initialize(Fr.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return 0;
	UINT w, h;
	if (FAILED(Conv->GetSize(&w, &h)) || w == 0 || h == 0) return 0;
	W = (int)w, H = (int)h, Pix.resize((size_t)W * H * 4);
	return SUCCEEDED(Conv->CopyPixels(nullptr, w * 4, (UINT)Pix.size(), Pix.data()));
}
bool Save() {
	ComPtr<IWICImagingFactory> F;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&F)))) return 0;
	GUID Fmt = GUID_NULL;
	size_t d = Out.find_last_of(L'.');
	if (d == wstring::npos) return 0;
	wstring e = Out.substr(d + 1);
	for (auto& c : e) c = (wchar_t)std::towlower(c);
	if (e == L"png") Fmt = GUID_ContainerFormatPng;
	else if (e == L"jpg" || e == L"jpeg") Fmt = GUID_ContainerFormatJpeg;
	else if (e == L"bmp") Fmt = GUID_ContainerFormatBmp;
	else if (e == L"tif" || e == L"tiff") Fmt = GUID_ContainerFormatTiff;
	else return 0;
	bool Jp = Fmt == GUID_ContainerFormatJpeg;
	if (Jp && (W > 65535 || H > 65535)) return 0;
	ComPtr<IWICStream> S;
	if (FAILED(F->CreateStream(&S))) return 0;
	if (FAILED(S->InitializeFromFilename(Out.c_str(), GENERIC_WRITE))) return 0;
	ComPtr<IWICBitmapEncoder> E;
	if (FAILED(F->CreateEncoder(Fmt, nullptr, &E))) return 0;
	if (FAILED(E->Initialize(S.Get(), WICBitmapEncoderNoCache))) return 0;
	ComPtr<IWICBitmapFrameEncode> Fr;
	ComPtr<IPropertyBag2> P;
	if (FAILED(E->CreateNewFrame(&Fr, &P))) return 0;
	if (Jp && P) {
		PROPBAG2 O = {};
		O.pstrName = (LPOLESTR)L"ImageQuality";
		VARIANT V; VariantInit(&V);
		V.vt = VT_R4, V.fltVal = 1.0f;
		P->Write(1, &O, &V);
	}
	if (FAILED(Fr->Initialize(P.Get()))) return 0;
	if (FAILED(Fr->SetSize(W, H))) return 0;
	WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
	vector<unsigned char> T;
	const unsigned char* src = Res.data();
	UINT st = (UINT)W * 4;
	if (Jp) {
		T.resize((size_t)W * H * 3);
		for (size_t i = 0, j = 0; i < Res.size(); i += 4, j += 3) {
			T[j] = Res[i], T[j + 1] = Res[i + 1], T[j + 2] = Res[i + 2];
		}
		src = T.data(), st = (UINT)W * 3, pf = GUID_WICPixelFormat24bppBGR;
	}
	WICPixelFormatGUID req = pf;
	if (FAILED(Fr->SetPixelFormat(&pf)) || !IsEqualGUID(pf, req)) return 0;
	if (FAILED(Fr->WritePixels((UINT)H, st, st * (UINT)H, (BYTE*)src))) return 0;
	if (FAILED(Fr->Commit())) return 0;
	return SUCCEEDED(E->Commit());
}
void Run() {
	size_t n = (size_t)W * H;
	Gilbert(W, H);
	size_t off = n > 1 ? (size_t)(Hash(Pwd) % (n - 1)) + 1 : 0;
	Res.assign(Pix.size(), 0);
	for (size_t i = 0; i < n; i++) {
		size_t j = i + off; if (j >= n) j -= n;
		size_t op = (size_t)Cv[i] * 4, np = (size_t)Cv[j] * 4;
		if (Enc) memcpy(&Res[np], &Pix[op], 4);
		else memcpy(&Res[op], &Pix[np], 4);
	}
}
int wmain(int argc, wchar_t* argv[]) {
	for (int i = 1; i < argc; i++) {
		wstring a = argv[i];
		if (a == L"-c") Enc = 1;
		else if (a == L"-d") Dec = 1;
		else if (a == L"-i" && i + 1 < argc) In = argv[++i];
		else if (a == L"-o" && i + 1 < argc) Out = argv[++i];
		else if (a == L"-p" && i + 1 < argc) Pwd = argv[++i];
		else { wcerr << L"Bad arg: " << a.c_str() << L'\n'; return 1; }
	}
	if (Enc == Dec || In.empty() || Out.empty()) {
		wcerr << L"Usage: -c|-d -i <in> -o <out> [-p <pwd>]\n";
		return 1;
	}
	if (lstrcmpiW(In.c_str(), Out.c_str()) == 0) { wcerr << L"In=Out\n"; return 1; }
	if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
	if (!Load()) { wcerr << L"Load fail\n"; CoUninitialize(); return 1; }
	Run();
	bool ok = Save();
	CoUninitialize();
	if (!ok) { wcerr << L"Save fail\n"; return 1; }
	return 0;
}
