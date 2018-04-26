/*!
 * \brief
 * IDXGISwapChainVtbl interfaces used by DX10 and DX11
 *
 * \file
 *
 * This file defines all of the IDXGISwapChain::Present, SetFullscreenState,
 * and Release interfaces that are overriden by this source file.
 *
 * \copyright
 * CopyRight 1993-2016 NVIDIA Corporation.  All rights reserved.
 * NOTICE TO LICENSEE: This source code and/or documentation ("Licensed Deliverables")
 * are subject to the applicable NVIDIA license agreement
 * that governs the use of the Licensed Deliverables.
 */

#include <windows.h>
#include <Psapi.h>
#include <d3d11.h>
#include <d3d10.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <algorithm>
#include "IDXGISwapChain.h"
#include "NvIFREncoderDXGI.h"
#include "ReplaceVtbl.h"
#include "Logger.h"

extern simplelogger::Logger *logger;
extern AppParam *pAppParam;

static IDXGISwapChainVtbl vtbl;

std::vector<NvIFREncoder*> pEncoderArray;
std::vector<IDXGISwapChain*> SwapChainArray;

int numPlayers = 0;

int index = 0;

LONGLONG g_llBegin = 0;
LONGLONG g_llPerfFrequency = 0;
BOOL g_timeInitialized = FALSE;

#define QPC(Int64) QueryPerformanceCounter((LARGE_INTEGER*)&Int64)
#define QPF(Int64) QueryPerformanceFrequency((LARGE_INTEGER*)&Int64)

double GetFloatingDate()
{
    LONGLONG llNow;

    if (!g_timeInitialized)
    {
        QPC(g_llBegin);
        QPF(g_llPerfFrequency);
        g_timeInitialized = TRUE;
    }
    QPC(llNow);
    return(((double)(llNow - g_llBegin) / (double)g_llPerfFrequency));
}

inline HWND GetOutputWindow(IDXGISwapChain * This) 
{
    // Does not run at all
    //LOG_INFO(logger, "GetOutputWindow(IDXGISwapChain * This) : " << This);
    DXGI_SWAP_CHAIN_DESC desc;
    vtbl.GetDesc(This, &desc);
    return desc.OutputWindow;
}

static HRESULT STDMETHODCALLTYPE IDXGISwapChain_Present_Proxy(IDXGISwapChain * This, UINT SyncInterval, UINT Flags) 
{
    // "This" is different for each window. 
    // We can do a comparison checking "This", then storing that and using 
    // that variable to keep track of which variable belongs to which window.    
    IUnknown *pIUnkown;
    index = std::find(SwapChainArray.begin(), SwapChainArray.end(), This) - SwapChainArray.begin();
    vtbl.GetDevice(This, __uuidof(pIUnkown), reinterpret_cast<void **>(&pIUnkown));

    //ID3D10Device *pD3D10Device;
    ID3D11Device *pD3D11Device;

    //if (pIUnkown->QueryInterface(__uuidof(pD3D10Device), (void **)&pD3D10Device) == S_OK) {
    //    ID3D10Texture2D *pBackBuffer;
    //    vtbl.GetBuffer(This, 0, __uuidof(pBackBuffer), reinterpret_cast<void**>(&pBackBuffer));
    //    D3D10_TEXTURE2D_DESC desc;
    //    pBackBuffer->GetDesc(&desc);
    //
    //    if (pEncoder && !pEncoder->CheckSize(desc.Width, desc.Height)) {
    //        LOG_INFO(logger, "destroy d3d10 encoder, new size: " << desc.Width << "x" << desc.Height);
    //        delete pEncoder;
    //        pEncoder = NULL;
    //    }
    //    if (!pEncoder && !(pAppParam && pAppParam->bDwm) 
    //            && !(pAppParam && pAppParam->bForceHwnd && (HWND)pAppParam->hwnd != GetOutputWindow(This))) {
    //        /* On Windows 7, we can QueryInterface() to tell whether this is a D3D10.1 device and 
    //           prefer KeyedMutex resource when yes. But on Windows 8.1, any D3D10 device has D3D10.1
    //           interface, but the queried interface still can't open KeyedMutex resource. So for
    //           compatibility, we stick to non-KeyedMutex resource without checking D3D10.1.*/
    //        pEncoder = new NvIFREncoderDXGI<ID3D10Device, ID3D10Texture2D>(This, desc.Width, desc.Height, 
    //            desc.Format, FALSE, pAppParam);
    //
    //        if (!pEncoder->StartEncoder()) {
    //            LOG_WARN(logger, "failed to start d3d10 encoder");
    //            delete pEncoder;
    //            pEncoder = NULL;
    //        }
    //    }
    //    if (pEncoder) {
    //        if (!((NvIFREncoderDXGI<ID3D10Device, ID3D10Texture2D> *)pEncoder)->UpdateSharedSurface(pD3D10Device, pBackBuffer)) {
    //            LOG_WARN(logger, "d3d10 UpdateSharedSurface failed");
    //        }
    //    }
    //
    //    pBackBuffer->Release();
    //    pD3D10Device->Release();
    //
    //} 
    // Unreal Engine uses d3d11. d3d10 code is not run.
    
    if (pIUnkown->QueryInterface(__uuidof(pD3D11Device), (void **)&pD3D11Device) == S_OK) {
        ID3D11Texture2D *pBackBuffer;

        // This is definitely the line that determines what goes into the pEncoder. 
        // Swapping *This with Window0 toggles the swapping between windows or not.
        vtbl.GetBuffer(This, 0, __uuidof(pBackBuffer), reinterpret_cast<void**>(&pBackBuffer));
        D3D11_TEXTURE2D_DESC desc;
        pBackBuffer->GetDesc(&desc);

        if (pEncoderArray[index] && !pEncoderArray[index]->CheckSize(desc.Width, desc.Height)) {
            LOG_INFO(logger, "destroy d3d11 encoder, new size: " << desc.Width << "x" << desc.Height);
            //delete pEncoder0;
            pEncoderArray[index] = NULL;
        }

        // This only runs once at the very beginning (startup code)
        if (!pEncoderArray[index] && !(pAppParam && pAppParam->bDwm)
            && !(pAppParam && pAppParam->bForceHwnd && (HWND)pAppParam->hwnd != GetOutputWindow(This))) {

            LOG_INFO(logger, "Window size: " << desc.Width << "x" << desc.Height);
            pEncoderArray[index] = new NvIFREncoderDXGI<ID3D11Device, ID3D11Texture2D>(This, desc.Width, desc.Height,
                desc.Format, FALSE, pAppParam);

            if (!pEncoderArray[index]->StartEncoder(index, desc.Width, desc.Height)) {
                LOG_WARN(logger, "failed to start d3d11 encoder");
                //delete pEncoder0;
                pEncoderArray[index] = NULL;
            }
        }

        if (pEncoderArray[index]) {
            // The pEncoder probably receives the pBackBuffer data here every frame.
            if (!((NvIFREncoderDXGI<ID3D11Device, ID3D11Texture2D> *)pEncoderArray[index])->UpdateSharedSurface(pD3D11Device, pBackBuffer)) {
                LOG_WARN(logger, "d3d11 UpdateSharedSurface failed");
            }
        }
        pBackBuffer->Release();
        pD3D11Device->Release();
    }
    else {
        LOG_ERROR(logger, "D3DxDevice not supported for Window");
        return vtbl.Present(This, SyncInterval, Flags);
    }
    pIUnkown->Release();

    return vtbl.Present(This, 0, Flags);
}

static HRESULT STDMETHODCALLTYPE IDXGISwapChain_SetFullscreenState_Proxy(IDXGISwapChain * This, 
    BOOL Fullscreen, IDXGIOutput *pTarget)
{
    // Does not run at all. In windowed mode probably.
    //LOG_INFO(logger, "IDXGISwapChain_SetFullscreenState_Proxy(IDXGISwapChain * This) : " << This);
    return vtbl.SetFullscreenState(This, FALSE, pTarget);
}

static ULONG STDMETHODCALLTYPE IDXGISwapChain_Release_Proxy(IDXGISwapChain * This)
{
    // Runs once when windows are opened. 
    // 1st window opened: prints 1st window
    // 2nd window opened: prints 1st and 2nd window
    //LOG_INFO(logger, "IDXGISwapChain_Release_Proxy(IDXGISwapChain * This) : " << This);

    // If no elements match, SwapChainArray.end() is returned.
    if (std::find(SwapChainArray.begin(), SwapChainArray.end(), This) == SwapChainArray.end()) {
        SwapChainArray.push_back(This);
        static NvIFREncoder *pEncoder;
        pEncoderArray.push_back(pEncoder);
		numPlayers++;
    }
    
    LOG_TRACE(logger, __FUNCTION__);
    vtbl.AddRef(This);
    ULONG uRef = vtbl.Release(This) - 1;
    if (pEncoderArray[0] != NULL && uRef == 0 && pEncoderArray.size() != 0 && --numPlayers <= 0)
    {
        LOG_DEBUG(logger, "delete pEncoder0 in Release(), pEncoder0=" << pEncoderArray[0]);
        delete pEncoderArray[0];
    }
    return vtbl.Release(This);
}

static void SetProxy(IDXGISwapChainVtbl *pVtbl)
{
    pVtbl->Present = IDXGISwapChain_Present_Proxy;
    pVtbl->SetFullscreenState = IDXGISwapChain_SetFullscreenState_Proxy;
    pVtbl->Release = IDXGISwapChain_Release_Proxy;
}

BOOL IDXGISwapChain_ReplaceVtbl(IDXGISwapChain *pSwapChain) 
{
    // Runs after each IDXGIFactory1_CreateSwapChain_Proxy.
    // 1st and 3rd time it runs matches the *This.
    // 2nd time different.
    //LOG_INFO(logger, "IDXGISwapChain_ReplaceVtbl(IDXGISwapChain * pSwapChain) : " << pSwapChain);
    return ReplaceVtblEntries<IDXGISwapChainVtbl>(pSwapChain);
}
