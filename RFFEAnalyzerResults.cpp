#include "RFFEAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "RFFEAnalyzer.h"
#include "RFFEAnalyzerSettings.h"
#include <iostream>
#include <sstream>

static const char *RffeTypeStringShort[] =
{
    "EW",
    "-",
    "ER",
    "ELW",
    "ELR",
    "W",
    "R",
    "W0",
};

static const char *RffeTypeStringMid[] =
{
    "ExtWr",
    "Rsv",
    "ExtRd",
    "ExtLngWr",
    "ExtLngRd",
    "Wr",
    "Rd",
    "Wr0",
};


RFFEAnalyzerResults::RFFEAnalyzerResults( RFFEAnalyzer* analyzer, RFFEAnalyzerSettings* settings )
:	AnalyzerResults(),
	mSettings( settings ),
	mAnalyzer( analyzer )
{
}

RFFEAnalyzerResults::~RFFEAnalyzerResults()
{
}

void RFFEAnalyzerResults::GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base )
{
    channel = channel;

	ClearResultStrings();
	Frame frame = GetFrame( frame_index );

    switch( frame.mType )
    {
    case RffeSSCField:
        {
            AddResultString( "SSC" );
        }
        break;

    case RffeSAField:
        {
            char number_str[8];
		    std::stringstream ss;

		    AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 4, number_str, 8 );

            AddResultString( "SA" );

		    ss << "SA:" << number_str;
		    AddResultString( ss.str().c_str() );
        }
        break;

    case RffeTypeField:
        {
            AddResultString( RffeTypeStringShort[frame.mData1] );
            AddResultString( RffeTypeStringMid[frame.mData1] );
        }
        break;

    case RffeExByteCountField:
        {
            char number_str[8];
		    std::stringstream ss;

		    AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 4, number_str, 8 );

            AddResultString( "BC" );

		    ss << "BC:" << number_str;
		    AddResultString( ss.str().c_str() );
        }
        break;

    case RffeExLongByteCountField:
        {
            char number_str[8];
		    std::stringstream ss;

		    AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 3, number_str, 8 );

            AddResultString( "BC" );

		    ss << "BC:" << number_str;
		    AddResultString( ss.str().c_str() );
            ss.str("");
        }
        break;

    case RffeShortAddressField:
        {
            char number_str[8];
		    std::stringstream ss;

		    AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 5, number_str, 8 );

            AddResultString( "A" );

		    ss << "A:" << number_str;
		    AddResultString( ss.str().c_str() );
        }
        break;

    case RffeAddressField:
        {
            char number_str[8];
		    std::stringstream ss;

		    AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 8 );

            AddResultString( "A" );

		    ss << "A:" << number_str;
		    AddResultString( ss.str().c_str() );
        }
        break;

    case RffeShortDataField:
        {
            char number_str[8];
		    std::stringstream ss;

		    AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 7, number_str, 8 );

            AddResultString( "D" );

		    ss << "D:" << number_str;
		    AddResultString( ss.str().c_str() );
        }
        break;

    case RffeDataField:
        {
            char number_str[8];
		    std::stringstream ss;

		    AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 8 );

            AddResultString( "D" );

		    ss << "D:" << number_str;
		    AddResultString( ss.str().c_str() );
        }
        break;

    case RffeParityField:
        {
            char number_str[4];
		    std::stringstream ss;

		    AnalyzerHelpers::GetNumberString( frame.mData1, Decimal, 1, number_str, 4 );

            AddResultString( "P" );

		    ss << "P" << number_str;
		    AddResultString( ss.str().c_str() );
        }
        break;

    case RffeBusParkField:
        {
            AddResultString( "B" );
            AddResultString( "BP" );
        }
        break;

    case RffeErrorCaseField:
    default:
        {
            AddResultString( "E" );
        }
        break;
    }
}

void RFFEAnalyzerResults::GenerateExportFile( const char* file,
                                              DisplayBase display_base,
                                              U32 export_type_user_id )
{
    std::stringstream ss;
	void* f = AnalyzerHelpers::StartFile( file );
    bool sdata_used = true;

    export_type_user_id = export_type_user_id;

	if( mSettings->mSdataChannel == UNDEFINED_CHANNEL )
		sdata_used = false;

	U64 trigger_sample  = mAnalyzer->GetTriggerSample();
	U32 sample_rate     = mAnalyzer->GetSampleRate();

	ss << "Time [s],Packet ID,SA,Type,CmdParity,BC,Address,Data" << std::endl;

	U64 num_frames = GetNumFrames();
	U64 num_packets = GetNumPackets();
	for( U32 i=0; i < num_packets; i++ )
	{
        U64 first_frame_id;
        U64 last_frame_id;

		GetFramesContainedInPacket( i, &first_frame_id, &last_frame_id );
		Frame frame = GetFrame( first_frame_id );
		
        // time
		char time_str[128];
		AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive,
                                        trigger_sample,
                                        sample_rate,
                                        time_str,
                                        128 );

        // package id
        char packet_str[128];
		AnalyzerHelpers::GetNumberString( i, Decimal, 0, packet_str, 128 );

        // start of seq control
        U64 frame_id = first_frame_id;
		frame = GetFrame( frame_id );
        frame_id++;

        // slave address
		frame = GetFrame( frame_id );
		char sa_str[128];
		AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 4, sa_str, 128 );
        frame_id++;

        // type
		frame = GetFrame( frame_id );
		char type_str[128];
		AnalyzerHelpers::GetNumberString( frame.mData2, display_base, 4, type_str, 128 );

        frame_id++;

        ss << time_str << "," << packet_str << std::endl;

        // Parity
        // byte count
        // address
        // data

		if( UpdateExportProgressAndCheckForCancel( i, num_packets ) == true )
		{
			AnalyzerHelpers::EndFile( f );
			return;
		}
    }

	UpdateExportProgressAndCheckForCancel( num_frames, num_frames );
	AnalyzerHelpers::EndFile( f );
}

void RFFEAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
	//Frame frame = GetFrame( frame_index );
    frame_index = frame_index;
    display_base = display_base;

	ClearResultStrings();
	AddResultString( "not supported yet" );
}

void RFFEAnalyzerResults::GeneratePacketTabularText( U64 packet_id, DisplayBase display_base )
{
    packet_id = packet_id;
    display_base = display_base;

	ClearResultStrings();
	AddResultString( "not supported yet" );
}

void RFFEAnalyzerResults::GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base )
{
    transaction_id = transaction_id;
    display_base = display_base;

	ClearResultStrings();
	AddResultString( "not supported yet" );
}