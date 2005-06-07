/**
*  Demux.cpp
*  Copyright (C) 2004-2005 bear
*
*  This file is part of TSFileSource, a directshow push source filter that
*  provides an MPEG transport stream output.
*
*  TSFileSource is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  TSFileSource is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with TSFileSource; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*  bear can be reached on the forums at
*    http://forums.dvbowners.com/
*/

#include <streams.h>
#include "bdaiface.h"
#include "ks.h"
#include "ksmedia.h"
#include "bdamedia.h"
#include "mediaformats.h"

#include "Demux.h"

//*********************************************************************************************
//NP Control Additions

#include "Bdatif.h"
#include "tuner.h"
#include <commctrl.h>
#include <atlbase.h>

//NP Slave Additions

#include "TunerEvent.h"

//*********************************************************************************************

 Demux::Demux(PidParser *pPidParser) :
	m_bAuto(TRUE),
	m_bMPEG2AudioMediaType(TRUE),

//*********************************************************************************************
//Audio2 Additions

	m_bMPEG2Audio2Mode(FALSE),

//NP Control Additions

	m_bNPControl(TRUE),

//NP Slave Additions

	m_bNPSlave(FALSE),
	OnConnectBusyFlag(false),

//*********************************************************************************************

	m_WasPlaying(FALSE),
	m_bAC3Mode(TRUE),
	m_bCreateTSPinOnDemux(FALSE)
{
	m_pPidParser = pPidParser;
//*********************************************************************************************
//Bug fix

	m_pGraphBuilder = NULL;
	m_pFilterChain = NULL;
	m_pMediaControl = NULL;

//*********************************************************************************************

}

Demux::~Demux()
{
//*********************************************************************************************
//Bug fix

	if (m_pMediaControl != NULL){m_pMediaControl->Release(); m_pMediaControl = NULL;}
	if (m_pFilterChain != NULL){m_pFilterChain->Release();	m_pFilterChain = NULL;}
	if (m_pGraphBuilder != NULL){m_pGraphBuilder->Release(); m_pGraphBuilder = NULL;}

//*********************************************************************************************

}

HRESULT Demux::AOnConnect(IFilterGraph *pGraph)
{

//*********************************************************************************************
//Bug fix

	if (m_pGraphBuilder == NULL)

//*********************************************************************************************

		if(FAILED(pGraph->QueryInterface(IID_IGraphBuilder, (void **) &m_pGraphBuilder)))
		{
			return S_FALSE;
		}

//*********************************************************************************************
//Bug fix

	if (m_pFilterChain == NULL)

//*********************************************************************************************

		if(FAILED(pGraph->QueryInterface(IID_IFilterChain, (void **) &m_pFilterChain)))
		{
			m_pGraphBuilder->Release();
			return S_FALSE;
		}

//*********************************************************************************************
//Bug fix

	if (m_pMediaControl == NULL)

//*********************************************************************************************

	// if there is no streaming, the following variables will not be initialized.
	if(m_pGraphBuilder != NULL){
		if(FAILED(m_pGraphBuilder->QueryInterface(IID_IMediaControl, (void **) &m_pMediaControl)))
		{
			m_pFilterChain->Release();
			m_pGraphBuilder->Release();
			return S_FALSE;
		}
	}

//*********************************************************************************************
//NP Control & Slave Additions
	if (OnConnectBusyFlag)
		return S_FALSE;

	// Check if Enabled
	if (!m_bAuto && !m_bNPControl && !m_bNPSlave)
		return S_FALSE;

	OnConnectBusyFlag = true;

//Removed	if (!m_bAuto)
//Removed		return S_FALSE;

//*********************************************************************************************

	m_WasPlaying = FALSE;
	m_TimeOut[0] = 0;
	m_TimeOut[1] = 0;
	if (IsPlaying() == S_OK)
	{
		m_TimeOut[0] = 10000;
		m_WasPlaying = TRUE;
	}

// declare local variables
	IEnumFilters* EnumFilters;
//Removed as not needed	IBaseFilter* m_pDemux = NULL;

//*********************************************************************************************
//NP Control Additions

	// Parse only the existing Network Provider Filter
	// in the filter graph, we do this by looking for filters
	// that implement the ITuner interface while
	// the count is still active.
	if(SUCCEEDED(pGraph->EnumFilters(&EnumFilters)))
	{
		IBaseFilter* m_pNetworkProvider;
		ULONG Fetched(0);
		while(EnumFilters->Next(1, &m_pNetworkProvider, &Fetched) == S_OK)
		{
			if(m_pNetworkProvider != NULL)
			{
				UpdateNetworkProvider(m_pNetworkProvider);
	
				m_pNetworkProvider->Release();
				m_pNetworkProvider = NULL;
			}
		}
		EnumFilters->Release();
	}

//*********************************************************************************************

	// Parse only the existing Mpeg2 Demultiplexer Filter
	// in the filter graph, we do this by looking for filters
	// that implement the IMpeg2Demultiplexer interface while
	// the count is still active.
	if(SUCCEEDED(pGraph->EnumFilters(&EnumFilters)))
	{
		IBaseFilter* m_pDemux;
		ULONG Fetched(0);
		while(EnumFilters->Next(1, &m_pDemux, &Fetched) == S_OK)
		{
			if(m_pDemux != NULL)
			{
				// TODO: Change this so that only the first demux filter that is connected gets updated
				UpdateDemuxPins(m_pDemux);

				m_pDemux->Release();
				m_pDemux = NULL;
			}
		}
		EnumFilters->Release();
	}

	if (m_WasPlaying && IsPlaying() == S_FALSE)
	{
		//Re Start the Graph if was running and was stopped.
		if (DoStart() == S_OK)
		{
			while(IsPlaying() == S_FALSE)
			{
				if (Sleeps(100,m_TimeOut) != S_OK)
				{
					break;
				}
			}
		}
	}

//	m_pMediaControl->Release();
//	m_pMediaControl = NULL;
//	m_pFilterChain->Release();
//	m_pFilterChain = NULL;
//	m_pGraphBuilder->Release();
//	m_pGraphBuilder = NULL;

//*********************************************************************************************
//NP Control Additions

	OnConnectBusyFlag = false;

//*********************************************************************************************

	return NOERROR;
}


HRESULT Demux::UpdateDemuxPins(IBaseFilter* pDemux)
{
	HRESULT hr = E_INVALIDARG;

	if(pDemux == NULL)
		return hr;

//*********************************************************************************************
//NP Control Additions

	// Check if Enabled
	if (!m_bAuto)
	{
		return S_FALSE;
	}

//*********************************************************************************************

	// Get an instance of the Demux control interface
	IMpeg2Demultiplexer* muxInterface = NULL;
	if(SUCCEEDED(pDemux->QueryInterface (&muxInterface)))
	{

//TCHAR sz[100];
//sprintf(sz, "%u", 0);
//MessageBox(NULL, sz, TEXT("UpdateDemuxPins"), MB_OK);
//*********************************************************************************************
//TIF Additions

		// Update TIF Pin
//		if (SUCCEEDED(CheckTIFPin(pDemux))){
			// If TIF was found

		
//		} //Just wanted to see if this would work and it did.
	
//*********************************************************************************************

		// Update Video Pin
		if (FAILED(CheckVideoPin(pDemux))){
			// If no Video Pin was found
			hr = NewVideoPin(muxInterface, L"Video");
		}

		// If we have AC3 preference
		if (m_bAC3Mode){
			// Update AC3 Pin
			if (FAILED(CheckAC3Pin(pDemux))){
				// If no AC3 Pin found
				if (FAILED(CheckAudioPin(pDemux))){
					// If no MP1/2 Pin found
					if (FAILED(NewAC3Pin(muxInterface, L"Audio"))){
						// If unable to create AC3 pin
						NewAudioPin(muxInterface, L"Audio");
					}
				}
				else{
					// If we do have an mp1/2 audio pin
					USHORT pPid;
					//GetAudioPid(&pPid);
					pPid = get_MP2AudioPid();
					if (!pPid){
						// If we don't have a mp1/2 audio pid
						//GetAC3Pid(&pPid);
//**********************************************************************************************
//Audio2 AC3 Additions
						pPid = get_AC3_2AudioPid();
//Removed						pPid = m_pPidParser->pids.ac3;
//**********************************************************************************************
						if (pPid && FAILED(CheckAC3Pin(pDemux))){
							// change pin type if we do have a AC3 pid & can don't already have an AC3 pin
							LPWSTR PinName = L"Audio";
							BOOL connect = FALSE;
							ChangeDemuxPin(pDemux, &PinName, &connect);
							if (FAILED(CheckAC3Pin(pDemux))){
								// if we can't change the pin type to AC3 make a new pin
								muxInterface->DeleteOutputPin(PinName);
								NewAC3Pin(muxInterface, PinName);
							}
							if (connect){
								// If old pin was already connected
								IPin* pIPin;
								if (SUCCEEDED(pDemux->FindPin(PinName, &pIPin))){
									// Reconnect pin
									m_pGraphBuilder->Render(pIPin);
									pIPin->Release();
								}
							}
						}
					}
				}
			}
			else{
				// If we already have a AC3 Pin
				USHORT pPid;
				//GetAC3Pid(&pPid);
//**********************************************************************************************
//Audio2 AC3 Additions
				pPid = get_AC3_2AudioPid();
//Removed				pPid = m_pPidParser->pids.ac3;
//**********************************************************************************************
				if (!pPid){
					// If we don't have a AC3 Pid
					//GetAudioPid(&pPid);
					pPid = get_MP2AudioPid();
					if (pPid && FAILED(CheckAudioPin(pDemux))){
						// change pin type if we do have a mp1/2 pid & can don't already have an mp1/2 pin
						LPWSTR PinName = L"Audio";
						BOOL connect = FALSE;
						ChangeDemuxPin(pDemux, &PinName, &connect);
						if (FAILED(CheckAudioPin(pDemux))){
							// if we can't change the pin type to mp2 make a new pin
							muxInterface->DeleteOutputPin(PinName);
							NewAudioPin(muxInterface, PinName);
						}
						if (connect){
							// If old pin was already connected
							IPin* pIPin;
							if (SUCCEEDED(pDemux->FindPin(PinName, &pIPin))){
								// Reconnect pin
								m_pGraphBuilder->Render(pIPin);
								pIPin->Release();
							}
						}
					}
				}
			}

		} //If AC3 is not prefered
		else if (FAILED(CheckAudioPin(pDemux))){
			// If no mp1/2 audio Pin found
			if (FAILED(CheckAC3Pin(pDemux))){
				// If no AC3 audio Pin found
				if (FAILED(NewAudioPin(muxInterface, L"Audio"))){
					// If unable to create mp1/2 pin
					hr = NewAC3Pin(muxInterface, L"Audio");
				}
			}
			else{
				// If we already have a AC3 Pin
				USHORT pPid;
				//GetAC3Pid(&pPid);
//**********************************************************************************************
//Audio2 AC3 Additions
				pPid = get_AC3_2AudioPid();
//Removed				pPid = m_pPidParser->pids.ac3;
//**********************************************************************************************
				if (!pPid){
					// If we don't have a AC3 Pid
					//GetAudioPid(&pPid);
					pPid = get_MP2AudioPid();
					if (pPid && FAILED(CheckAudioPin(pDemux))){
						// change pin type if we do have a mp1/2 pid & can don't already have an mp1/2 pin
						LPWSTR PinName = L"Audio";
						BOOL connect = FALSE;
						ChangeDemuxPin(pDemux, &PinName, &connect);
						if (FAILED(CheckAudioPin(pDemux))){
							// if we can't change the pin type to mp2 make a new pin
							muxInterface->DeleteOutputPin(PinName);
							NewAudioPin(muxInterface, PinName);
						}
						if (connect){
							// If old pin was already connected
							IPin* pIPin;
							if (SUCCEEDED(pDemux->FindPin(PinName, &pIPin))){
								// Reconnect pin
								m_pGraphBuilder->Render(pIPin);
								pIPin->Release();
							}
						}
					}
				}
			}
		}
		else{
				// If we do have an mp1/2 audio pin
				USHORT pPid;
				//GetAudioPid(&pPid);
				pPid = get_MP2AudioPid();
				if (!pPid){
					// If we don't have a mp1/2 Pid
					//GetAC3Pid(&pPid);
//**********************************************************************************************
//Audio2 AC3 Additions
					pPid = get_AC3_2AudioPid();
//Removed					pPid = m_pPidParser->pids.ac3;
//**********************************************************************************************
					if (pPid && FAILED(CheckAC3Pin(pDemux))){
						// change pin type if we do have a AC3 pid & can don't already have an AC3 pin
						LPWSTR PinName = L"Audio";
						BOOL connect = FALSE;
						ChangeDemuxPin(pDemux, &PinName, &connect);
						if (FAILED(CheckAC3Pin(pDemux))){
							// if we can't change the pin type to AC3 make a new pin
							muxInterface->DeleteOutputPin(PinName);
							NewAudioPin(muxInterface, PinName);
						}
						if (connect){
							// If old pin was already connected
							IPin* pIPin;
							if (SUCCEEDED(pDemux->FindPin(PinName, &pIPin))){
								// Reconnect pin
								m_pGraphBuilder->Render(pIPin);
								pIPin->Release();
							}
						}
					}
				}
		}
		// Update Transport Stream Pin
		if (m_bCreateTSPinOnDemux)
		{
			if (FAILED(CheckTsPin(pDemux))){
				// If no Transport Stream Pin was found
				hr = NewTsPin(muxInterface, L"TS");
			}
		}

		// Update Teletext Pin
		if (FAILED(CheckTelexPin(pDemux))){
			// If no Teletext Pin was found
			hr = NewTelexPin(muxInterface, L"Teletext");
		}

		muxInterface->Release();
	}
	return hr;
}

HRESULT Demux::CheckDemuxPin(IBaseFilter* pDemux, AM_MEDIA_TYPE pintype, IPin** pIPin)
{
	HRESULT hr = E_INVALIDARG;

	if(pDemux == NULL && *pIPin != NULL){return hr;}

	IPin* pDPin;
	PIN_DIRECTION  direction;
	AM_MEDIA_TYPE *type;

	// Enumerate the Demux pins
	IEnumPins* pIEnumPins;
	if (SUCCEEDED(pDemux->EnumPins(&pIEnumPins))){

		ULONG pinfetch(0);
		while(pIEnumPins->Next(1, &pDPin, &pinfetch) == S_OK){

			hr = pDPin->QueryDirection(&direction);
			if(direction == PINDIR_OUTPUT){

				IEnumMediaTypes* ppEnum;
				if (SUCCEEDED(pDPin->EnumMediaTypes(&ppEnum))){

					ULONG fetched(0);
					while(ppEnum->Next(1, &type, &fetched) == S_OK)
					{

						if (type->majortype == pintype.majortype
							&& type->subtype == pintype.subtype
							&& type->formattype == pintype.formattype){

							*pIPin = pDPin;
							ppEnum->Release();
							return S_OK;
						}
						type = NULL;
					}
					ppEnum->Release();
					ppEnum = NULL;
				}
			}
			pDPin = NULL;
		}
		pIEnumPins->Release();
   }
	return E_FAIL;
}

//*********************************************************************************************
//NP Control & Slave Additions

HRESULT Demux::UpdateNetworkProvider(IBaseFilter* pNetworkProvider)
{
	HRESULT hr = E_INVALIDARG;

	if(pNetworkProvider == NULL)
		return hr;

	// Check if Enabled
	if (!m_bNPControl && !m_bNPSlave)
	{
		return S_FALSE;
	}

	ITuner* pITuner;
    hr = pNetworkProvider->QueryInterface(__uuidof (ITuner), reinterpret_cast <void**> (&pITuner));
    if(SUCCEEDED (hr))
    {
//TCHAR sz[100];
//sprintf(sz, "%u", 0);
//MessageBox(NULL, sz, TEXT("UpdateNetworkProvider"), MB_OK);
		//Setup to get the tune request
		CComPtr <ITuneRequest> pNewTuneRequest;			
		CComQIPtr <IDVBTuneRequest> pDVBTTuneRequest (pNewTuneRequest);
		hr = pITuner->get_TuneRequest(&pNewTuneRequest);
		pDVBTTuneRequest = pNewTuneRequest;

		//Test if we are in NP Slave mode
		if (m_bNPSlave && !m_bNPControl)
		{
			long Onid = 0;
			pDVBTTuneRequest->get_ONID(&Onid);
			long Tsid = 0;
			pDVBTTuneRequest->get_TSID(&Tsid);

			if (Onid != m_pPidParser->m_ONetworkID && Tsid != m_pPidParser->m_TStreamID)
			{
//TCHAR sz[100];
//sprintf(sz, "%u", 0);
//MessageBox(NULL, sz, TEXT("RefreshPids"), MB_OK);
//				m_pPidParser->RefreshPids();
			
			}

			long Sid = 0;
			pDVBTTuneRequest->get_SID(&Sid); //Get the SID from the NP

			m_pPidParser->m_ProgramSID = Sid; //Set the prefered SID
			m_pPidParser->set_ProgramSID(); //Update the Pids
		}

		if (m_bNPControl)
		{
			//Must be in control mode to get this far then setup the NP tune request
			if (m_pPidParser->m_ONetworkID == 0)
				pDVBTTuneRequest->put_ONID((long)-1);
			else
				pDVBTTuneRequest->put_ONID((long)m_pPidParser->m_ONetworkID);

			if (m_pPidParser->pids.sid == 0)
				pDVBTTuneRequest->put_SID((long)-1);
			else
				pDVBTTuneRequest->put_SID((long)m_pPidParser->pids.sid);
			
			if (m_pPidParser->m_TStreamID == 0)
				pDVBTTuneRequest->put_TSID((long)-1);
			else
				pDVBTTuneRequest->put_TSID((long)m_pPidParser->m_TStreamID);

//			hr = pDVBTTuneRequest->QueryInterface(&pNewTuneRequest);//IID_ITuneRequest,(void **)
			//If the tune request is valid then tune.
			if (pITuner->Validate(pNewTuneRequest) == S_OK)
			{
				hr = pITuner->put_TuneRequest(pNewTuneRequest);
			}
		}
		pDVBTTuneRequest.Release();
		pNewTuneRequest.Release();
		pITuner->Release();
//TCHAR sz[100];
//sprintf(sz, "%u", 0);
//MessageBox(NULL, sz, TEXT("put_TuneRequest"), MB_OK);
	}
	return hr;
}

//Stop TIF Additions
HRESULT Demux::SetTIFState(IFilterGraph *pGraph, REFERENCE_TIME tStart)
{
	HRESULT hr = S_FALSE;

	// declare local variables
	IEnumFilters* EnumFilters;

	// Parse only the existing TIF Filter
	// in the filter graph, we do this by looking for filters
	// that implement the GuideData interface while
	// the count is still active.
	if(SUCCEEDED(pGraph->EnumFilters(&EnumFilters)))
	{
		IBaseFilter* m_pTIF;
		ULONG Fetched(0);
		while(EnumFilters->Next(1, &m_pTIF, &Fetched) == S_OK)
		{
			if(m_pTIF != NULL)
			{
				// Get the GuideData interface from the TIF filter
				IGuideData* pGuideData;
				hr = m_pTIF->QueryInterface(&pGuideData);
				if (SUCCEEDED(hr))
				{
					if (tStart == 0)
					{
						m_pTIF->Pause();
						m_pTIF->Stop();
					}
					else
						m_pTIF->Run(tStart);

					pGuideData->Release();
				}
	
				m_pTIF->Release();
				m_pTIF = NULL;
			}
		}
		EnumFilters->Release();
	}
	return hr;
}

//TIF Additions
HRESULT Demux::CheckTIFPin(IBaseFilter* pDemux)
{
	HRESULT hr = E_INVALIDARG;

	if(pDemux == NULL){return hr;}

	AM_MEDIA_TYPE pintype;
	GetTIFMedia(&pintype);

	IPin* pOPin = NULL;
	if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pOPin)))
	{

		IPin* pIPin = NULL;
		if (SUCCEEDED(pOPin->ConnectedTo(&pIPin)))
		{

			PIN_INFO pinInfo;
			if (SUCCEEDED(pIPin->QueryPinInfo(&pinInfo)))
			{
		
				IBaseFilter* m_pTIF;
				m_pTIF = pinInfo.pFilter;

				// Get the GuideData interface from the TIF filter
				IGuideData* pGuideData;
				hr = m_pTIF->QueryInterface(&pGuideData);
				if (SUCCEEDED(hr))
				{
					// Get the TuneRequestinfo interface from the TIF filter
					CComPtr <ITuneRequestInfo> pTuneRequestInfo;
					hr = m_pTIF->QueryInterface(&pTuneRequestInfo);
					if (SUCCEEDED(hr))
					{

						// Get a list of services from the GuideData interface
						IEnumTuneRequests*  piEnumTuneRequests; 
						hr = pGuideData->GetServices(&piEnumTuneRequests); 
						if (SUCCEEDED(hr))
						{
							bool foundSID = false;
							while (!foundSID)
							{

							unsigned long ulRetrieved = 1;
							ITuneRequest *pTuneRequest = NULL; 

							while (SUCCEEDED(piEnumTuneRequests->Next(1, &pTuneRequest, &ulRetrieved)) && ulRetrieved > 0)
							{

								// Fill in the Components lists for the tune request
								hr = pTuneRequestInfo->CreateComponentList(pTuneRequest);
								if(SUCCEEDED(hr))
								{

									CComPtr <IComponents> pConponents;
									hr = pTuneRequest->get_Components(&pConponents);
									if(SUCCEEDED(hr))
									{

									    CComPtr<IEnumComponents> pEnum;
										hr = pConponents->EnumComponents(&pEnum);

										if (SUCCEEDED(hr))
										{
											CComPtr <IComponent> pComponent;
											ULONG cFetched = 1;

											while (SUCCEEDED(pEnum->Next(1, &pComponent, &cFetched)) && cFetched > 0)
											{

												CComPtr <IMPEG2Component> mpegComponent;
												hr = pComponent.QueryInterface(&mpegComponent);

												long progSID = -1;
												mpegComponent->get_ProgramNumber(&progSID);
												if (progSID == m_pPidParser->pids.sid)
													foundSID = true;

//				TCHAR sz[100];
//				sprintf(sz, "%u", progSID);
//				MessageBox(NULL, sz, TEXT("progSID"), MB_OK);

												mpegComponent.Release();
											}
											pComponent.Release();
										}
										pEnum.Release();
										pConponents.Release();
									}
									pTuneRequest->Release();
								}
							}
							piEnumTuneRequests->Release();
							}
						}
						pTuneRequestInfo.Release();
					}
					pGuideData->Release();
				}
//				m_pTIF->Stop();
				m_pTIF->Release();
			}
			pIPin->Release();
		}
		pOPin->Release();
		return S_OK;
	}
	return hr;
}

//*********************************************************************************************

HRESULT Demux::CheckVideoPin(IBaseFilter* pDemux)
{
	HRESULT hr = E_INVALIDARG;

	if(pDemux == NULL){return hr;}

	AM_MEDIA_TYPE pintype;
	GetVideoMedia(&pintype);

	IPin* pIPin = NULL;
	if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){

		USHORT pPid;
		//GetVideoPid(&pPid);
		pPid = m_pPidParser->pids.vid;
		if SUCCEEDED(LoadMediaPin(pIPin, pPid)){
			pIPin->Release();
			return S_OK;
		}
	}
	return hr;
}

HRESULT Demux::CheckAudioPin(IBaseFilter* pDemux)
{
	HRESULT hr = E_INVALIDARG;

	if(pDemux == NULL){return hr;}

	AM_MEDIA_TYPE pintype;
	GetMP1Media(&pintype);

	IPin* pIPin = NULL;
	if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){

		USHORT pPid;
		//GetAudioPid(&pPid);
		pPid = get_MP2AudioPid();
		if (SUCCEEDED(LoadMediaPin(pIPin, pPid))){
			pIPin->Release();
			return S_OK;
		};
	}
	else{

			GetMP2Media(&pintype);
			if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){

				USHORT pPid;
				//GetAudioPid(&pPid);
				pPid = get_MP2AudioPid();
				if (SUCCEEDED(LoadMediaPin(pIPin, pPid))){
					pIPin->Release();
					return S_OK;
				};
			};
		};
		return hr;
}

HRESULT Demux::CheckAC3Pin(IBaseFilter* pDemux)
{
	HRESULT hr = E_INVALIDARG;

	if(pDemux == NULL){return hr;}

	AM_MEDIA_TYPE pintype;
	GetAC3Media(&pintype);

	IPin* pIPin = NULL;
	if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){

		USHORT pPid;
		//GetAC3Pid(&pPid);
//**********************************************************************************************
//Audio2 AC3 Additions
		pPid = get_AC3_2AudioPid();
//Removed	pPid = m_pPidParser->pids.ac3;
//**********************************************************************************************
		if (SUCCEEDED(LoadMediaPin(pIPin, pPid))){
			pIPin->Release();
			return S_OK;
		}
	}
	return hr;
}

HRESULT Demux::CheckTelexPin(IBaseFilter* pDemux)
{
	HRESULT hr = E_INVALIDARG;

	if(pDemux == NULL){return hr;}

	AM_MEDIA_TYPE pintype;
	GetTelexMedia(&pintype);

	IPin* pIPin = NULL;
	if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){

		USHORT pPid;
		//GetTelexPid(&pPid);
		pPid = m_pPidParser->pids.txt;
		if (SUCCEEDED(LoadTelexPin(pIPin, pPid))){
			pIPin->Release();
			return S_OK;
		}
	}
	return hr;
}

HRESULT Demux::CheckTsPin(IBaseFilter* pDemux)
{
	HRESULT hr = E_INVALIDARG;

	if(pDemux == NULL){return hr;}

	AM_MEDIA_TYPE pintype;
	GetTSMedia(&pintype);

	IPin* pIPin = NULL;
	if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){

		if (SUCCEEDED(LoadTsPin(pIPin))){
			m_pGraphBuilder->Reconnect(pIPin);
			pIPin->Release();
			return S_OK;
		}
	}
	return hr;
}

HRESULT Demux::NewTsPin(IMpeg2Demultiplexer* muxInterface, LPWSTR pinName)
{
	HRESULT hr = E_INVALIDARG;

	if(muxInterface == NULL){return hr;}

	// Create out new pin of type GUID_NULL
	AM_MEDIA_TYPE type;
	GetTSMedia(&type);

	IPin* pIPin = NULL;
	if(SUCCEEDED(muxInterface->CreateOutputPin(&type, pinName ,&pIPin)))
	{
		hr = LoadTsPin(pIPin);
		pIPin->Release();
		hr = S_OK;
	}
	return hr;
}

HRESULT Demux::NewVideoPin(IMpeg2Demultiplexer* muxInterface, LPWSTR pinName)
{
	USHORT pPid;
	//GetVideoPid( &pPid);
	pPid = m_pPidParser->pids.vid;

	HRESULT hr = E_INVALIDARG;

	if(muxInterface == NULL || pPid == 0){return hr;}

	// Create out new pin of type GUID_NULL
	AM_MEDIA_TYPE type;
	ZeroMemory(&type, sizeof(AM_MEDIA_TYPE));
	GetVideoMedia(&type);

	IPin* pIPin = NULL;
	if(SUCCEEDED(muxInterface->CreateOutputPin(&type, pinName ,&pIPin)))
	{
		hr = LoadMediaPin(pIPin, (ULONG)pPid);
		pIPin->Release();
		hr = S_OK;
	}
	return hr;
}

HRESULT Demux::NewAudioPin(IMpeg2Demultiplexer* muxInterface, LPWSTR pinName)
{
	USHORT pPid;
	//GetAudioPid(&pPid);
	pPid = get_MP2AudioPid();

	HRESULT hr = E_INVALIDARG;

	if(muxInterface == NULL || pPid == 0){return hr;}

	// Create out new pin of type GUID_NULL
	AM_MEDIA_TYPE type;
	ZeroMemory(&type, sizeof(AM_MEDIA_TYPE));
	if (m_bMPEG2AudioMediaType)
		GetMP2Media(&type);
	else
		GetMP1Media(&type);

	IPin* pIPin = NULL;
	if(SUCCEEDED(muxInterface->CreateOutputPin(&type, pinName ,&pIPin)))
	{
		hr = LoadMediaPin(pIPin, (ULONG)pPid);
		pIPin->Release();
		hr = S_OK;
	}
	return hr;
}

HRESULT Demux::NewAC3Pin(IMpeg2Demultiplexer* muxInterface, LPWSTR pinName)
{
	USHORT pPid;
	//GetAC3Pid( &pPid);
//**********************************************************************************************
//Audio2 AC3 Additions
	pPid = get_AC3_2AudioPid();
//Removed	pPid = m_pPidParser->pids.ac3;
//**********************************************************************************************

	HRESULT hr = E_INVALIDARG;

	if(muxInterface == NULL || pPid == 0){return hr;}

	// Create out new pin of type GUID_NULL
	AM_MEDIA_TYPE type;
	GetAC3Media(&type);

	IPin* pIPin = NULL;
	if(SUCCEEDED(muxInterface->CreateOutputPin(&type, pinName ,&pIPin)))
	{
		hr = LoadMediaPin(pIPin, (ULONG)pPid);
		pIPin->Release();
		hr = S_OK;
	}
	return hr;
}

HRESULT Demux::NewTelexPin(IMpeg2Demultiplexer* muxInterface, LPWSTR pinName)
{
	USHORT pPid;
	//GetTelexPid(&pPid);
	pPid = m_pPidParser->pids.txt;

	HRESULT hr = E_INVALIDARG;

	if(muxInterface == NULL || pPid == 0){return hr;}

	// Create out new pin of type GUID_NULL
	AM_MEDIA_TYPE type;
	GetTelexMedia(&type);

	IPin* pIPin = NULL;
	if(SUCCEEDED(muxInterface->CreateOutputPin(&type, pinName ,&pIPin)))
	{
		hr = LoadTelexPin(pIPin, (ULONG)pPid);
		pIPin->Release();
		hr = S_OK;
	}
	return hr;
}

HRESULT Demux::LoadTsPin(IPin* pIPin)
{
	HRESULT hr = E_INVALIDARG;

	if(pIPin == NULL){return hr;}

	ClearDemuxPin(pIPin);

	// Get the Pid Map interface of the pin
	// and map the pids we want.
	IMPEG2PIDMap* muxMapPid;
	if(SUCCEEDED(pIPin->QueryInterface (&muxMapPid)))
	{
		//ULONG pPidArray[15];
		//hr = GetTsArray(&pPidArray[0]);
		//ULONG count = pPidArray[0];
		//muxMapPid->MapPID(count,&pPidArray[1] , MEDIA_TRANSPORT_PACKET);
		muxMapPid->MapPID(m_pPidParser->pids.TsArray[0], m_pPidParser->pids.TsArray, MEDIA_TRANSPORT_PACKET);
		muxMapPid->Release();
		hr = S_OK;

	}
	return hr;
}

HRESULT Demux::LoadMediaPin(IPin* pIPin, ULONG pid)
{
	HRESULT hr = E_INVALIDARG;

	if(pIPin == NULL){return hr;}

	ClearDemuxPin(pIPin);

	// Get the Pid Map interface of the pin
	// and map the pids we want.
	IMPEG2PIDMap* muxMapPid;
	if(SUCCEEDED(pIPin->QueryInterface (&muxMapPid)))
	{
		if (pid){
			muxMapPid->MapPID(1, &pid , MEDIA_ELEMENTARY_STREAM);
		}
		muxMapPid->Release();
		hr = S_OK;
	}
	return hr;
}

HRESULT Demux::LoadTelexPin(IPin* pIPin, ULONG pid)
{
	HRESULT hr = E_INVALIDARG;

	if(pIPin == NULL){return hr;}

	ClearDemuxPin(pIPin);

	// Get the Pid Map interface of the pin
	// and map the pids we want.
	IMPEG2PIDMap* muxMapPid;
	if(SUCCEEDED(pIPin->QueryInterface (&muxMapPid)))
	{
		if (pid){
			muxMapPid->MapPID(1, &pid , MEDIA_TRANSPORT_PACKET); //MEDIA_MPEG2_PSI
		}
		muxMapPid->Release();
		hr = S_OK;
	}
	return hr;
}

HRESULT Demux::ClearDemuxPin(IPin* pIPin)
{
	HRESULT hr = E_INVALIDARG;

	if(pIPin == NULL){return hr;}

	IMPEG2PIDMap* muxMapPid;
	if(SUCCEEDED(pIPin->QueryInterface (&muxMapPid))){

		IEnumPIDMap *pIEnumPIDMap;
		if (SUCCEEDED(muxMapPid->EnumPIDMap(&pIEnumPIDMap))){
			ULONG pNumb = 0;
			PID_MAP pPidMap;
			while(pIEnumPIDMap->Next(1, &pPidMap, &pNumb) == S_OK){
				ULONG pid = pPidMap.ulPID;
				hr = muxMapPid->UnmapPID(1, &pid);
			}
		}
		muxMapPid->Release();
	}
	return S_OK;
}

HRESULT Demux::ChangeDemuxPin(IBaseFilter* pDemux, LPWSTR* pPinName, BOOL* pConnect)

{
	HRESULT hr = E_INVALIDARG;

	if(pDemux == NULL || pConnect == NULL || pPinName == NULL){return hr;}

	// Get an instance of the Demux control interface
	IMpeg2Demultiplexer* muxInterface = NULL;
	if(SUCCEEDED(pDemux->QueryInterface (&muxInterface)))
	{
		char audiocheck[128];
		char videocheck[128];
		char telexcheck[128];
		char tscheck[128];
		char pinname[128];
		wcscpy((wchar_t*)pinname, *pPinName);
		wcscpy((wchar_t*)audiocheck, L"Audio");
		wcscpy((wchar_t*)videocheck, L"Video");
		wcscpy((wchar_t*)telexcheck, L"Teletext");
		wcscpy((wchar_t*)tscheck, L"TS");

		if (strcmp(audiocheck, pinname) == 0){

			AM_MEDIA_TYPE pintype;
			GetAC3Media(&pintype);
			IPin* pIPin = NULL;

				if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){

					ClearDemuxPin(pIPin);
					pIPin->QueryId(pPinName);

						IPin* pInpPin;
						if (SUCCEEDED(pIPin->ConnectedTo(&pInpPin))){

							PIN_INFO pinInfo;
							pInpPin->QueryPinInfo(&pinInfo);
							pinInfo.pFilter->Release();
							pInpPin->Release();

							if (m_WasPlaying){

								if (DoPause() == S_OK){while(IsPaused() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
								if (DoStop() == S_OK){while(IsStopped() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
							}

							if (SUCCEEDED(pIPin->Disconnect())){
								*pConnect = TRUE;
								m_pFilterChain->RemoveChain(pinInfo.pFilter, NULL);
							}
						}

						if (m_bMPEG2AudioMediaType)
							GetMP2Media(&pintype);
						else
							GetMP1Media(&pintype);

//						GetMP2Media(&pintype);
						muxInterface->SetOutputPinMediaType(*pPinName, &pintype);
						USHORT pPid;
						//GetAudioPid(&pPid);
						pPid = get_MP2AudioPid();
						LoadMediaPin(pIPin, pPid);
						pIPin->Release();
						hr = S_OK;
				}
				else{

					GetMP2Media(&pintype);
					if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){
						//ClearDemuxPin(pIPin);

						pIPin->QueryId(pPinName);
						IPin* pInpPin;

						if (SUCCEEDED(pIPin->ConnectedTo(&pInpPin))){

							PIN_INFO pinInfo;
							pInpPin->QueryPinInfo(&pinInfo);
							pinInfo.pFilter->Release();
							pInpPin->Release();

							if (m_WasPlaying){

								if (DoPause() == S_OK){while(IsPaused() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
								if (DoStop() == S_OK){while(IsStopped() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
							}

							if (SUCCEEDED(pIPin->Disconnect())){
								*pConnect = TRUE;
								m_pFilterChain->RemoveChain(pinInfo.pFilter, NULL);
							}
						}

						GetAC3Media(&pintype);
						muxInterface->SetOutputPinMediaType(*pPinName, &pintype);
						USHORT pPid;
						//GetAC3Pid(&pPid);
//**********************************************************************************************
//Audio2 AC3 Additions
						pPid = get_AC3_2AudioPid();
//Removed					pPid = m_pPidParser->pids.ac3;
//**********************************************************************************************
						LoadMediaPin(pIPin, pPid);
						pIPin->Release();
						hr = S_OK;
					}
					else{
						GetMP1Media(&pintype);
						if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){
							ClearDemuxPin(pIPin);
							pIPin->QueryId(pPinName);

							IPin* pInpPin;
							if (SUCCEEDED(pIPin->ConnectedTo(&pInpPin))){

								*pConnect = TRUE;
								PIN_INFO pinInfo;
								pInpPin->QueryPinInfo(&pinInfo);
								pinInfo.pFilter->Release();
								pInpPin->Release();

								if (m_WasPlaying){

									if (DoPause() == S_OK){while(IsPaused() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
									if (DoStop() == S_OK){while(IsStopped() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
								}

								if (SUCCEEDED(pIPin->Disconnect())){
									*pConnect = TRUE;
									m_pFilterChain->RemoveChain(pinInfo.pFilter, NULL);
								}
							}

							GetAC3Media(&pintype);
							muxInterface->SetOutputPinMediaType(*pPinName, &pintype);
							USHORT pPid;
							//GetAC3Pid(&pPid);
//**********************************************************************************************
//Audio2 AC3 Additions
							pPid = get_AC3_2AudioPid();
//Removed							pPid = m_pPidParser->pids.ac3;
//**********************************************************************************************

							LoadMediaPin(pIPin, pPid);
							pIPin->Release();
							hr = S_OK;
						}
					}
				}
				return hr;
		}
		else if (strcmp(videocheck, pinname) == 0){

			AM_MEDIA_TYPE pintype;
			GetVideoMedia(&pintype);

			IPin* pIPin = NULL;
			if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){
				ClearDemuxPin(pIPin);
				pIPin->QueryId(pPinName);
				IPin* pInpPin;
				if (SUCCEEDED(pIPin->ConnectedTo(&pInpPin))){

					PIN_INFO pinInfo;
					pInpPin->QueryPinInfo(&pinInfo);
					pinInfo.pFilter->Release();
					pInpPin->Release();

					if (m_WasPlaying){

						if (DoPause() == S_OK){while(IsPaused() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
						if (DoStop() == S_OK){while(IsStopped() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
					}

					if (SUCCEEDED(pIPin->Disconnect())){
						*pConnect = TRUE;
						m_pFilterChain->RemoveChain(pinInfo.pFilter, NULL);
					}
				}

				muxInterface->DeleteOutputPin(*pPinName);
				pIPin->Release();
				hr = S_OK;
			}
		}
		else if (strcmp(telexcheck, pinname) == 0){

			AM_MEDIA_TYPE pintype;
			GetTelexMedia(&pintype);

			IPin* pIPin = NULL;
			if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){
				ClearDemuxPin(pIPin);
				pIPin->QueryId(pPinName);

				IPin* pInpPin;
				if (SUCCEEDED(pIPin->ConnectedTo(&pInpPin))){

					PIN_INFO pinInfo;
					pInpPin->QueryPinInfo(&pinInfo);
					pinInfo.pFilter->Release();
					pInpPin->Release();

					if (m_WasPlaying){

						if (DoPause() == S_OK){while(IsPaused() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
						if (DoStop() == S_OK){while(IsStopped() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
					}

					if (SUCCEEDED(pIPin->Disconnect())){
						*pConnect = TRUE;
						m_pFilterChain->RemoveChain(pinInfo.pFilter, NULL);
					}
				}

				pIPin->Disconnect();
				muxInterface->DeleteOutputPin(*pPinName);
				pIPin->Release();
				hr = S_OK;
			}
		}
		else if (strcmp(tscheck, pinname) == 0){

			AM_MEDIA_TYPE pintype;
			GetTSMedia(&pintype);

			IPin* pIPin = NULL;
			if (SUCCEEDED(CheckDemuxPin(pDemux, pintype, &pIPin))){
				ClearDemuxPin(pIPin);
				pIPin->QueryId(pPinName);

				IPin* pInpPin;
				if (SUCCEEDED(pIPin->ConnectedTo(&pInpPin))){

					*pConnect = TRUE;
					PIN_INFO pinInfo;
					pInpPin->QueryPinInfo(&pinInfo);
					pinInfo.pFilter->Release();
					pInpPin->Release();

					if (m_WasPlaying){

						if (DoPause() == S_OK){while(IsPaused() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
						if (DoStop() == S_OK){while(IsStopped() == S_FALSE){if (Sleeps(100,m_TimeOut) != S_OK) break;}}
					}

					if (SUCCEEDED(pIPin->Disconnect())){
						*pConnect = TRUE;
						m_pFilterChain->RemoveChain(pinInfo.pFilter, NULL);
					}
				}
				pIPin->Disconnect();
				muxInterface->DeleteOutputPin(*pPinName);
				pIPin->Release();
				hr = S_OK;
			}
		}
		muxInterface->Release();
	}
		return hr;
}

HRESULT Demux::GetAC3Media(AM_MEDIA_TYPE *pintype)
{
	HRESULT hr = E_INVALIDARG;

	if(pintype == NULL){return hr;}

	ZeroMemory(pintype, sizeof(AM_MEDIA_TYPE));
	pintype->majortype = MEDIATYPE_Audio;
	pintype->subtype = MEDIASUBTYPE_DOLBY_AC3;
	pintype->cbFormat = sizeof(MPEG1AudioFormat);
	pintype->pbFormat = MPEG1AudioFormat;
	pintype->bFixedSizeSamples = TRUE;
	pintype->bTemporalCompression = 0;
	pintype->lSampleSize = 1;
	pintype->formattype = FORMAT_WaveFormatEx;
	pintype->pUnk = NULL;

	return S_OK;
}

HRESULT Demux::GetMP2Media(AM_MEDIA_TYPE *pintype)

{
	HRESULT hr = E_INVALIDARG;

	if(pintype == NULL){return hr;}

	ZeroMemory(pintype, sizeof(AM_MEDIA_TYPE));
	pintype->majortype = MEDIATYPE_Audio;
	pintype->subtype = MEDIASUBTYPE_MPEG2_AUDIO; //MEDIASUBTYPE_MPEG1Payload;
	pintype->formattype = FORMAT_WaveFormatEx; //FORMAT_None; //
	pintype->cbFormat = sizeof(g_MPEG1AudioFormat); //Mpeg2ProgramVideo
	pintype->pbFormat = g_MPEG1AudioFormat; //;Mpeg2ProgramVideo
	pintype->bFixedSizeSamples = TRUE;
	pintype->bTemporalCompression = 0;
	pintype->lSampleSize = 1;
	pintype->pUnk = NULL;

	return S_OK;
}

HRESULT Demux::GetMP1Media(AM_MEDIA_TYPE *pintype)

{
	HRESULT hr = E_INVALIDARG;

	if(pintype == NULL){return hr;}

	ZeroMemory(pintype, sizeof(AM_MEDIA_TYPE));
	pintype->majortype = MEDIATYPE_Audio;
	pintype->subtype = MEDIASUBTYPE_MPEG1Payload;
	pintype->formattype = FORMAT_WaveFormatEx; //FORMAT_None; //
	pintype->cbFormat = sizeof(MPEG1AudioFormat);
	pintype->pbFormat = MPEG1AudioFormat;
	pintype->bFixedSizeSamples = TRUE;
	pintype->bTemporalCompression = 0;
	pintype->lSampleSize = 1;
	pintype->pUnk = NULL;

	return S_OK;
}

HRESULT Demux::GetVideoMedia(AM_MEDIA_TYPE *pintype)

{
	HRESULT hr = E_INVALIDARG;

	if(pintype == NULL){return hr;}

	ZeroMemory(pintype, sizeof(AM_MEDIA_TYPE));
	pintype->majortype = KSDATAFORMAT_TYPE_VIDEO;
	pintype->subtype = MEDIASUBTYPE_MPEG2_VIDEO;
	pintype->bFixedSizeSamples = TRUE;
	pintype->bTemporalCompression = 0;
	pintype->lSampleSize = 1;
	pintype->formattype = FORMAT_MPEG2Video;
	pintype->pUnk = NULL;
	pintype->cbFormat = sizeof(Mpeg2ProgramVideo);
	pintype->pbFormat = Mpeg2ProgramVideo;

	return S_OK;
}

//*********************************************************************************************
//TIF Additions

HRESULT Demux::GetTIFMedia(AM_MEDIA_TYPE *pintype)

{
	HRESULT hr = E_INVALIDARG;

	if(pintype == NULL){return hr;}

	ZeroMemory(pintype, sizeof(AM_MEDIA_TYPE));
	pintype->majortype = KSDATAFORMAT_TYPE_MPEG2_SECTIONS;
	pintype->subtype = MEDIASUBTYPE_DVB_SI; //MEDIASUBTYPE_MPEG2DATA; //Sections & tables
	pintype->formattype = KSDATAFORMAT_SPECIFIER_NONE;

	return S_OK;
}

//*********************************************************************************************

HRESULT Demux::GetTelexMedia(AM_MEDIA_TYPE *pintype)

{
	HRESULT hr = E_INVALIDARG;

	if(pintype == NULL){return hr;}

	ZeroMemory(pintype, sizeof(AM_MEDIA_TYPE));
	pintype->majortype = KSDATAFORMAT_TYPE_MPEG2_SECTIONS;
	pintype->subtype = KSDATAFORMAT_SUBTYPE_NONE; //MEDIASUBTYPE_None;
	pintype->formattype = KSDATAFORMAT_SPECIFIER_NONE; //GUID_NULL; //FORMAT_MPEGStreams

	return S_OK;
}

HRESULT Demux::GetTSMedia(AM_MEDIA_TYPE *pintype)

{
	HRESULT hr = E_INVALIDARG;

	if(pintype == NULL){return hr;}

	ZeroMemory(pintype, sizeof(AM_MEDIA_TYPE));
	pintype->majortype = MEDIATYPE_Stream;
	pintype->subtype = KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT; //MEDIASUBTYPE_MPEG2_TRANSPORT; //KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT;
	pintype->formattype = FORMAT_None; //GUID_NULL; //FORMAT_MPEGStreams; //

	return S_OK;
}

HRESULT Demux::Sleeps(ULONG Duration, long TimeOut[])
{
	HRESULT hr = S_OK;

	Sleep(Duration);
	TimeOut[0] = TimeOut[0] - Duration;
	if (TimeOut[0] <= 0)
	{
		hr = S_FALSE;
	}
	return hr;
}

HRESULT Demux::IsStopped()
{
	HRESULT hr;
	if (m_pMediaControl == NULL)
		hr = m_pGraphBuilder->QueryInterface (&m_pMediaControl);

	if (!m_pMediaControl)
	{
		return S_OK;
	}
	FILTER_STATE state;
	hr = m_pMediaControl->GetState(5000, (OAFilterState*)&state);
	if (hr == S_OK && state == State_Stopped){return S_OK;}
	if (hr == VFW_S_STATE_INTERMEDIATE)
	{
		return S_OK;
	}

	return S_FALSE;
}

HRESULT Demux::IsPlaying()
{
	HRESULT hr;
	if (m_pMediaControl == NULL)
		hr = m_pGraphBuilder->QueryInterface (&m_pMediaControl);

	if (!m_pMediaControl)
	{
		return S_OK;
	}
	FILTER_STATE state;
	hr = m_pMediaControl->GetState(5000, (OAFilterState*)&state);
	if (hr == S_OK && state == State_Running){return S_OK;}
	if (hr == VFW_S_STATE_INTERMEDIATE)
	{
		return S_OK;
	}
	return S_FALSE;
}

HRESULT Demux::IsPaused()
{
	HRESULT hr;
	if (m_pMediaControl == NULL)
		hr = m_pGraphBuilder->QueryInterface (&m_pMediaControl);

	if (!m_pMediaControl)
	{
		return S_OK;
	}
	FILTER_STATE state;
	hr = m_pMediaControl->GetState(5000, (OAFilterState*)&state);
	if (hr == S_OK && state == State_Paused){return S_OK;}
	if (hr == VFW_S_STATE_INTERMEDIATE)
	{
		return S_OK;
	}

	if (hr == VFW_S_CANT_CUE)
		return hr;


	return S_FALSE;
}

HRESULT Demux::DoStop()
{
	HRESULT hr;

	if (m_pMediaControl == NULL)
		hr = m_pGraphBuilder->QueryInterface (&m_pMediaControl);


	if (!m_pMediaControl)
	{
		return S_OK;
	}

	if (m_pMediaControl->Stop() != S_OK)
	{
		return S_OK;
	}
	return S_OK;
}

HRESULT Demux::DoStart()
{
	HRESULT hr;
	if (m_pMediaControl == NULL)
		hr = m_pGraphBuilder->QueryInterface (&m_pMediaControl);

	if (!m_pMediaControl)
	{
		return S_OK;
	}
	hr = m_pMediaControl->Run();
	if (FAILED(hr))
	{
		return S_OK;
	}
	return S_OK;
}

HRESULT Demux::DoPause()
{
	HRESULT hr;
	if (m_pMediaControl == NULL)
		hr = m_pGraphBuilder->QueryInterface (&m_pMediaControl);


	if (!m_pMediaControl)
	{
//		ErrorMessageBox(TEXT("Media Control interface is null"));
		return S_OK;
	}
	hr = m_pMediaControl->Pause();
	if (FAILED(hr))
	{
//		ErrorMessageBox(TEXT("Error stopping graph"));
		return S_OK;
	}
	return S_OK;
}

BOOL Demux::get_Auto()
{
	return m_bAuto;
}

void Demux::set_Auto(BOOL bAuto)
{
	m_bAuto = bAuto;
}

//*********************************************************************************************
//NP Control Additions

BOOL Demux::get_NPControl()
{
	return m_bNPControl;
}

void Demux::set_NPControl(BOOL bNPControl)
{
	m_bNPControl = bNPControl;
}

//NP Slave Additions

BOOL Demux::get_NPSlave()
{
	return m_bNPSlave;
}

void Demux::set_NPSlave(BOOL bNPSlave)
{
	m_bNPSlave = bNPSlave;
}

//*********************************************************************************************

BOOL Demux::get_AC3Mode()
{
	return m_bAC3Mode;
}

void Demux::set_AC3Mode(BOOL bAC3Mode)
{
	m_bAC3Mode = bAC3Mode;
}

BOOL Demux::get_CreateTSPinOnDemux()
{
	return m_bCreateTSPinOnDemux;
}

void Demux::set_CreateTSPinOnDemux(BOOL bCreateTSPinOnDemux)
{
	m_bCreateTSPinOnDemux = bCreateTSPinOnDemux;
}

BOOL Demux::get_MPEG2AudioMediaType()
{
	return m_bMPEG2AudioMediaType;
}

void Demux::set_MPEG2AudioMediaType(BOOL bMPEG2AudioMediaType)
{
	m_bMPEG2AudioMediaType = bMPEG2AudioMediaType;
}

//**********************************************************************************************
//Audio2 Additions

BOOL Demux::get_MPEG2Audio2Mode()
{
	return m_bMPEG2Audio2Mode;
}

void Demux::set_MPEG2Audio2Mode(BOOL bMPEG2Audio2Mode)
{
	m_bMPEG2Audio2Mode = bMPEG2Audio2Mode;
	return;
}

int Demux::get_MP2AudioPid()
{
	if (m_bMPEG2Audio2Mode && m_pPidParser->pids.aud2)
		return m_pPidParser->pids.aud2;
	else
		return m_pPidParser->pids.aud;
}

int Demux::get_AC3_2AudioPid()
{
	if (m_bMPEG2Audio2Mode && m_pPidParser->pids.ac3_2)
		return m_pPidParser->pids.ac3_2;
	else
		return m_pPidParser->pids.ac3;
}
//**********************************************************************************************










