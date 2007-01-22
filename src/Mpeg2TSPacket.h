/**
*  Mpeg2TransportStream.h
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

#ifndef MPEG2TSPACKET_H
#define MPEG2TSPACKET_H

#include <vector>
#include <list>

using namespace std;

#define TS_PACKET_SIZE 188
#define PS_PACKET_SIZE 2048

#define AFC_PAYLOAD                    1
#define AFC_ADAPTIONFIELD              2
#define ADAPTIONFIELDFOLLOWEDBYPAYLOAD 3

class Mpeg2TSPacket
{
public:
	Mpeg2TSPacket();
	virtual ~Mpeg2TSPacket();

	HRESULT ParseTSPacket(BYTE *pPacket, ULONG ulDataLength, __int64 filePosition);

	void Log();
	static void LogLegend();

public:
	BYTE sync_byte;
	BOOL transport_error_indicator;
	BOOL payload_unit_start_indicator;
	BOOL transport_priority;
	SHORT PID;
	BYTE transport_scrambling_control;
	BYTE adaption_field_control;
	BYTE continuity_counter;
	BYTE pointer_field;
	BYTE payload_start;

	struct adaption_field_tag
	{
		BYTE adaption_field_length;
		BOOL discontinuity_indicator;
		BOOL random_access_indicator;
		BOOL elementary_stream_priority_indicator;
		BOOL PCR_flag;
		BOOL OPCR_flag;
		BOOL splicing_point_flag;
		BOOL transport_private_data_flag;
		BOOL adaption_field_entention_flag;
		
		LONGLONG program_clock_reference_base;
		BYTE program_clock_reference_reserved;
		SHORT program_clock_reference_extention;

		LONGLONG original_program_clock_reference_base;
		BYTE original_program_clock_reference_reserved;
		SHORT original_program_clock_reference_extention;

		BYTE splice_countdown;

		BYTE transport_private_data_length;
		BYTE transport_private_data_start;

		struct adaption_field_extention_tag
		{
			BYTE adaption_field_extention_length;
			BOOL ltw_flag;
			BOOL piecewise_rate_flag;
			BOOL seamless_splice_flag;
			BYTE adaption_field_entention_reserved;

			BOOL ltw_valid_flag;
			SHORT ltw_offset;

			BYTE piecewise_rate_reserved;
			SHORT piecewise_rate;

			BYTE splice_type;
			BYTE DTS_next_AU_32_30;
			BOOL marker_bit1;
			SHORT DTS_next_AU_29_15;
			BOOL marker_bit2;
			SHORT DTS_next_AU_14_0;
			BOOL marker_bit3;

		} extention;
	} adaption_field;

	__int64 filePosition;
};


#endif
