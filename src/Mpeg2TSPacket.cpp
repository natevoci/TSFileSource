/**
*  Mpeg2TSPacket.cpp
*  Copyright (C) 2007      nate
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
*  bear and nate can be reached on the forums at
*    http://forums.dvbowners.com/
*/

#include <windows.h>
#include "Mpeg2TsPacket.h" 

extern void LogDebug(const char *fmt, ...) ;

Mpeg2TSPacket::Mpeg2TSPacket()
{
	sync_byte = 0;
	transport_error_indicator = 0;
	payload_unit_start_indicator = 0;
	transport_priority = 0;
	PID = 0;
	transport_scrambling_control = 0;
	adaption_field_control = 0;
	continuity_counter = 0;
	pointer_field = 0;
	payload_start = 0;

	memset(&adaption_field, 0, sizeof(adaption_field_tag));

	filePosition = 0;
}

Mpeg2TSPacket::~Mpeg2TSPacket(void)
{
}

HRESULT Mpeg2TSPacket::ParseTSPacket(BYTE *pPacket, ULONG ulDataLength, __int64 filePosition)
{
	//	transport_packet()
	//	{			
	//		sync_byte							8	bslbf
	//		transport_error_indicator			1	bslbf
	//		payload_unit_start_indicator		1	bslbf
	//		transport_priority					1	bslbf
	//		PID									13	uimsbf
	//		transport_scrambling_control		2	bslbf
	//		adaptation_field_control			2	bslbf
	//		continuity_counter					4	uimsbf
	//		if(adaptation_field_control=='10'  || adaptation_field_control=='11'){			
	//			adaptation_field()			
	//		}			
	//		if(adaptation_field_control=='01' || adaptation_field_control=='11') {			
	//			for (i=0;i<N;i++){			
	//				data_byte					8	bslbf
	//			}			
	//		}			
	//	}

	if (ulDataLength < TS_PACKET_SIZE)
		return S_FALSE;

	this->filePosition = filePosition;

	sync_byte = pPacket[0];
	if (sync_byte != 0x47)
	{
		transport_error_indicator = 1;
		return E_FAIL;
	}

	transport_error_indicator = ((pPacket[1] & 0x80) != 0);
	payload_unit_start_indicator = ((pPacket[1] & 0x40) != 0);
	transport_priority = ((pPacket[1] & 0x20) != 0);
	PID = ((pPacket[1] & 0x1f) << 8) | pPacket[2];
	transport_scrambling_control = (pPacket[3] & 0xc0) >> 6;
	adaption_field_control = (pPacket[3] & 0x30) >> 4;
	continuity_counter = pPacket[3] & 0x0f;
	payload_start = 4;

	if ((adaption_field_control & AFC_ADAPTIONFIELD) == AFC_ADAPTIONFIELD)
	{
		BYTE *pAF = pPacket + 4;
		adaption_field.adaption_field_length = pAF[0];

		payload_start += 1 + adaption_field.adaption_field_length;

		if (adaption_field.adaption_field_length > 0)
		{
			adaption_field.discontinuity_indicator = ((pAF[1] & 0x80) != 0);
			adaption_field.random_access_indicator = ((pAF[1] & 0x40) != 0);
			adaption_field.elementary_stream_priority_indicator = ((pAF[1] & 0x20) != 0);
			adaption_field.PCR_flag = ((pAF[1] & 0x10) != 0);
			adaption_field.OPCR_flag = ((pAF[1] & 0x08) != 0);
			adaption_field.splicing_point_flag = ((pAF[1] & 0x04) != 0);
			adaption_field.transport_private_data_flag = ((pAF[1] & 0x02) != 0);
			adaption_field.adaption_field_entention_flag = ((pAF[1] & 0x01) != 0);

			BYTE *pAFCurr = pAF + 2;
			if (adaption_field.PCR_flag == 1)
			{
				LONGLONG pcr_base = 0;
				pcr_base += pAFCurr[0];
				pcr_base = pcr_base << 8;
				pcr_base += pAFCurr[1];
				pcr_base = pcr_base << 8;
				pcr_base += pAFCurr[2];
				pcr_base = pcr_base << 8;
				pcr_base += pAFCurr[3];
				pcr_base = pcr_base << 1;
				pcr_base += ((pAFCurr[4] & 0x80) >> 7);

				adaption_field.program_clock_reference_base = pcr_base;
				adaption_field.program_clock_reference_reserved = ((pAFCurr[4] & 0x7e) >> 1);
				adaption_field.program_clock_reference_extention = ((pAFCurr[4] & 0x01) << 8) + pAFCurr[5];

				pAFCurr += 6;
			}
			if (adaption_field.OPCR_flag == 1)
			{
				LONGLONG pcr_base = 0;
				pcr_base += pAFCurr[0];
				pcr_base = pcr_base << 8;
				pcr_base += pAFCurr[1];
				pcr_base = pcr_base << 8;
				pcr_base += pAFCurr[2];
				pcr_base = pcr_base << 8;
				pcr_base += pAFCurr[3];
				pcr_base = pcr_base << 1;
				pcr_base += ((pAFCurr[4] & 0x80) >> 7);

				adaption_field.original_program_clock_reference_base = pcr_base;
				adaption_field.original_program_clock_reference_reserved = ((pAFCurr[4] & 0x7e) >> 1);
				adaption_field.original_program_clock_reference_extention = ((pAFCurr[4] & 0x01) << 8) + pAFCurr[5];

				pAFCurr += 6;
			}
			if (adaption_field.splicing_point_flag == 1)
			{
				adaption_field.splice_countdown = pAFCurr[0];
				pAFCurr += 1;
			}
			if (adaption_field.transport_private_data_flag == 1)
			{
				adaption_field.transport_private_data_length = pAFCurr[0];
				pAFCurr += 1;
				adaption_field.transport_private_data_start = pAFCurr - pPacket;
				pAFCurr += adaption_field.transport_private_data_length;
			}
			if (adaption_field.adaption_field_entention_flag == 1)
			{
				adaption_field.extention.adaption_field_extention_length = pAFCurr[0];
				adaption_field.extention.ltw_flag = ((pAFCurr[1] & 0x80) != 0);
				adaption_field.extention.piecewise_rate_flag = ((pAFCurr[1] & 0x40) != 0);
				adaption_field.extention.seamless_splice_flag = ((pAFCurr[1] & 0x20) != 0);
				adaption_field.extention.adaption_field_entention_reserved = ((pAFCurr[1] & 0x18) != 0);

				pAFCurr += 2;
				if (adaption_field.extention.ltw_flag == 1)
				{
					adaption_field.extention.ltw_valid_flag = ((pAFCurr[0] & 0x80) != 0);
					adaption_field.extention.ltw_offset += ((pAFCurr[0] & 0x7f) << 8) + pAFCurr[1];
					pAFCurr += 2;
				}
				if (adaption_field.extention.piecewise_rate_flag == 1)
				{
					adaption_field.extention.piecewise_rate_reserved = (pAFCurr[0] & 0xc0) >> 6;
					adaption_field.extention.piecewise_rate = ((pAFCurr[0] & 0x3F) << 16);
					adaption_field.extention.piecewise_rate += (pAFCurr[1] << 8);
					adaption_field.extention.piecewise_rate += pAFCurr[2];
					pAFCurr += 3;
				}
				if (adaption_field.extention.seamless_splice_flag == 1)
				{
					adaption_field.extention.splice_type = (pAFCurr[0] & 0xf0) >> 4;
					adaption_field.extention.DTS_next_AU_32_30 = (pAFCurr[0] & 0x0e) >> 1;
					adaption_field.extention.marker_bit1 = (pAFCurr[0] & 0x01);
					pAFCurr +=1;

					adaption_field.extention.DTS_next_AU_29_15 = (((WORD *)pAFCurr)[0] & 0xfffe) >> 1;
					adaption_field.extention.marker_bit2 = (((WORD *)pAFCurr)[0] & 0x0001);
					pAFCurr += 2;

					adaption_field.extention.DTS_next_AU_14_0 = (((WORD *)pAFCurr)[0] & 0xfffe) >> 1;
					adaption_field.extention.marker_bit3 = (((WORD *)pAFCurr)[0] & 0x0001);
					pAFCurr += 2;
				}
			}
		}
	}

	if (payload_unit_start_indicator)
	{
		pointer_field = pPacket[payload_start];
		payload_start += 1;
		pointer_field += payload_start;
	}

	return S_OK;
}

void Mpeg2TSPacket::Log()
{
	if (payload_unit_start_indicator || ((adaption_field_control & AFC_ADAPTIONFIELD) == AFC_ADAPTIONFIELD))
	{
		char line[1024];
		
		sprintf((char*)&line, "0x%.8x    %i %i %i %.4x %.2x %.2x %.1x",
			(LONG)filePosition,
			transport_error_indicator,
			payload_unit_start_indicator,
			transport_priority,
			PID,
			transport_scrambling_control,
			adaption_field_control,
			continuity_counter);
		
		if ((adaption_field_control & AFC_ADAPTIONFIELD) == AFC_ADAPTIONFIELD)
		{
			if (adaption_field.adaption_field_length > 0)
			{
				sprintf((char*)&line, "%s  %i %i %i %i %i %i %i %i", line,
					adaption_field.discontinuity_indicator,
					adaption_field.random_access_indicator,
					adaption_field.elementary_stream_priority_indicator,
					adaption_field.PCR_flag,
					adaption_field.OPCR_flag,
					adaption_field.splicing_point_flag,
					adaption_field.transport_private_data_flag,
					adaption_field.adaption_field_entention_flag);
				
				if (adaption_field.PCR_flag == 1)
				{
					//LONGLONG PCR = adaption_field.program_clock_reference_base * 300;
					//PCR += adaption_field.program_clock_reference_extention;
					//double totalSeconds = PCR / (double)27000000;

					LONGLONG PCR = adaption_field.program_clock_reference_base;
					double totalSeconds = PCR / (double)90000;
					long hours = long(totalSeconds/3600);
					long minutes = long(totalSeconds/60) - hours*60;
					double seconds = totalSeconds - minutes*60 - hours*3600;

					sprintf((char*)&line, "%s PCR-%.2u:%.2u:%.1u%f", line,
						hours,
						minutes,
						long(seconds)/10,
						seconds - double(long(seconds)/10)*10);
				}
				if (adaption_field.OPCR_flag == 1)
				{
					//LONGLONG PCR = adaption_field.original_program_clock_reference_base * 300;
					//PCR += adaption_field.original_program_clock_reference_extention;
					//double totalSeconds = PCR / (double)27000000;

					LONGLONG PCR = adaption_field.original_program_clock_reference_base;
					double totalSeconds = PCR / (double)90000;
					long hours = long(totalSeconds/3600);
					long minutes = long(totalSeconds/60) - hours*60;
					double seconds = totalSeconds - minutes*60 - hours*3600;

					sprintf((char*)&line, "%s PCR-%.2u:%.2u:%.1u%f", line,
						hours,
						minutes,
						long(seconds)/10,
						seconds - double(long(seconds)/10)*10);
				}
			}
		}
		
		if (transport_error_indicator)
		{
			sprintf((char*)&line, "%s  error bit set.", line);
		}

		LogDebug((char*)&line);
	}
}

void Mpeg2TSPacket::LogLegend()
{
	LogDebug(" er   = Error indicator bit");
	LogDebug(" st   = Payload unit start indicator bit");
	LogDebug(" prio = Priority bit");
	LogDebug(" tssc = Scrambling Control");
	LogDebug(" adfe = Adaption field control");
	LogDebug(" cont = Continuity counter");
	LogDebug(" dis  = Discontinuity indicator");
	LogDebug(" rand = Random Access Indicator");
	LogDebug(" prio = Elementary Stream Priority Indicator");
	LogDebug(" PCR  = PCR Flag");
	LogDebug(" OPCR = OPCR Flag");
	LogDebug(" spli = Splicing Point Flag");
	LogDebug(" priv = Transport Private Data Flag");
	LogDebug(" afex = Adaption Field Extention Flag\n");

	LogDebug(" Offset       e s p PID  ts ad c  d r p P O s p a   PCR    OPCR");
	LogDebug("              r t r      sc fe o  i a r C P p r f");
	LogDebug("                  i            n  s n i R C l i e");
	LogDebug("                  o            t    d o   R i v x");
	LogDebug("----------    - - - ---- -- -- -  - - - - - - - - ---------------");
}
