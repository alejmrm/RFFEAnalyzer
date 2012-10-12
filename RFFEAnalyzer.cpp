#include "RFFEAnalyzer.h"
#include "RFFEAnalyzerSettings.h"
#include "RFFEUtil.h"
#include <AnalyzerChannelData.h>


RFFEAnalyzer::RFFEAnalyzer()
:	Analyzer2(),  
	mSettings( new RFFEAnalyzerSettings() ),
	mSimulationInitilized( false )
{
	SetAnalyzerSettings( mSettings.get() );
}

RFFEAnalyzer::~RFFEAnalyzer()
{
	KillThread();
}

void RFFEAnalyzer::SetupResults()
{
	mResults.reset( new RFFEAnalyzerResults( this, mSettings.get() ) );
	SetAnalyzerResults( mResults.get() );
	mResults->AddChannelBubblesWillAppearOn( mSettings->mSdataChannel );
}

void RFFEAnalyzer::WorkerThread()
{
    U32 count;
	mSampleRateHz = GetSampleRate();

	mSdata = GetAnalyzerChannelData( mSettings->mSdataChannel );
	mSclk  = GetAnalyzerChannelData( mSettings->mSclkChannel );

    mResults->CancelPacketAndStartNewPacket();

	for( ; ; )
	{
        FindStartSeqCondition();
        count = FindSlaveAddrAndCommand();
        FindParity();

        switch ( mRffeType )
        {
        case RFFEAnalyzerResults::RffeTypeExtWrite:
            FindAddressFrame();
            for( U32 i = count ; i != 0; i-- )
            {
                FindDataFrame();
            }
            FindBusPark();
            break;
        case RFFEAnalyzerResults::RffeTypeReserved:
            break;
        case RFFEAnalyzerResults::RffeTypeExtRead:
            FindAddressFrame();
            FindBusPark();
            for( U32 i = count ; i != 0; i-- )
            {
                FindDataFrame();
            }
            FindBusPark();
            break;
        case RFFEAnalyzerResults::RffeTypeExtLongWrite:
            FindAddressFrame();
            FindAddressFrame();
            for( U32 i = count ; i != 0; i-- )
            {
                FindDataFrame();
            }
            FindBusPark();
            break;
        case RFFEAnalyzerResults::RffeTypeExtLongRead:
            FindAddressFrame();
            FindAddressFrame();
            FindBusPark();
            for( U32 i = count ; i != 0; i-- )
            {
                FindDataFrame();
            }
            FindBusPark();
            break;
        case RFFEAnalyzerResults::RffeTypeNormalWrite:
            FindDataFrame();
            FindBusPark();
            break;
        case RFFEAnalyzerResults::RffeTypeNormalRead:
            FindBusPark();
            FindDataFrame();
            FindBusPark();
            break;
        case RFFEAnalyzerResults::RffeTypeShortWrite:
            FindBusPark();
            break;
        }
        mResults->CommitPacketAndStartNewPacket();
        CheckIfThreadShouldExit();
	}
}

void RFFEAnalyzer::AdvanceToBeginningStartBit()
{
    U64 sample;
    BitState state;

	for( ; ; )
	{
		mSdata->AdvanceToNextEdge();
        state = mSdata->GetBitState();
		if( state == BIT_HIGH )
		{
            sample = mSdata->GetSampleNumber();
			mSclk->AdvanceToAbsPosition( sample );
            state  = mSclk->GetBitState();
			if( state == BIT_LOW )
				break;
		}	
	}
}

void RFFEAnalyzer::FindStartSeqCondition()
{
    U64 sample;
    BitState state;

    U64 sample_up;
    U64 sample_dn;
    U64 sample_next;
    U64 pulse_1, pulse_2;
    bool did_toggle;

    for ( ; ; )
    {
        sample = mSdata->GetSampleNumber();
        mSclk->AdvanceToAbsPosition( sample );
        state  = mSdata->GetBitState();
        if ( state == BIT_LOW )
        {
            AdvanceToBeginningStartBit();
        }
        else
        {
            mSdata->AdvanceToNextEdge();
            continue;
        }

        // advance the pulse
        sample_up = mSdata->GetSampleNumber();
        mSdata->AdvanceToNextEdge();
        sample_dn = mSdata->GetSampleNumber();
        pulse_1   = sample_dn - sample_up;

        // use to scan for more clocks ahead, in samples
        pulse_width2 = pulse_1 * 2;

        did_toggle = mSclk->WouldAdvancingToAbsPositionCauseTransition( sample_dn );
        if( did_toggle )
            continue; // error: found clk toggling

        // look for idle in clk & data signals
        mSclk->AdvanceToAbsPosition( sample_dn );
        sample_next = mSclk->GetSampleOfNextEdge();
        pulse_2     = sample_next - sample_dn;

        if ( pulse_2 < pulse_1 )
            continue; // error: idle period too short

        // at rising edge of clk
        mSclk->AdvanceToNextEdge();
        sample = mSclk->GetSampleNumber();
        mSdata->AdvanceToAbsPosition( sample );

		Frame frame;
        frame.mType                    = RFFEAnalyzerResults::RffeSSCField;
		frame.mStartingSampleInclusive = sample_up;
		frame.mEndingSampleInclusive   = sample;

        mResults->AddMarker( sample_up,
                             AnalyzerResults::Start,
                             mSettings->mSdataChannel );
		mResults->AddFrame( frame );
		mResults->CommitResults();

		ReportProgress( frame.mEndingSampleInclusive );
        break;
    }

    gSampleCount = 0;
}

void RFFEAnalyzer::DrawMarkersDotsAndStates( U32 start,
                                             U32 len,
                                             AnalyzerResults::MarkerType type,
                                             AnalyzerResults::MarkerType *states)
{
    for (U32 i=start; len--; i++ )
    {
        mResults->AddMarker( sampleClkOffsets[i],
                             type,
                             mSettings->mSclkChannel );
        mResults->AddMarker( sampleDataOffsets[i],
                             states[i],
                             mSettings->mSdataChannel );
    }
}

BitState RFFEAnalyzer::GetNextBit(U32 const idx, U64 *const clk, U64 *const data )
{
    BitState state;

    // at rising edge of clk
    clk[idx] =  mSclk->GetSampleNumber();
    gSampleClk[gSampleCount] = clk[idx] - gSampleNormalized;

    // advance to falling edge of sclk
    mSclk->AdvanceToNextEdge();
    data[idx] =  mSclk->GetSampleNumber();
    gsampleData[gSampleCount] = data[idx] - gSampleNormalized;
    gSampleCount++;

    mSdata->AdvanceToAbsPosition( data[idx] );
    state = mSdata->GetBitState();

    // at rising edge of clk
    mSclk->AdvanceToNextEdge();

    return state;
}

U32 RFFEAnalyzer::FindSlaveAddrAndCommand()
{
    U32 count = 0;
    U64 cmd;
    U32 i;
	DataBuilder cmd_builder;
    BitState state;
    AnalyzerResults::MarkerType sampleDataState[16];

    cmd_builder.Reset( &cmd, AnalyzerEnums::MsbFirst , 12 );

    // normalize samples for debugging
    gSampleNormalized = mSclk->GetSampleNumber();

    // starting at rising edge of clk
    for( i=0; i < 12; i++ )
    {
        state = GetNextBit( i, sampleClkOffsets,  sampleDataOffsets );
        cmd_builder.AddBit( state );

        if ( state == BIT_HIGH )
            sampleDataState[i] = AnalyzerResults::One;
        else
            sampleDataState[i] = AnalyzerResults::Zero;
    }
    sampleClkOffsets[i] =  mSclk->GetSampleNumber();

	Frame frame;
    // decode slave address
    frame.mType                    = RFFEAnalyzerResults::RffeSAField;
    frame.mData1                   = ( cmd & 0xF00 ) >> 8;
	frame.mStartingSampleInclusive = sampleClkOffsets[0];
	frame.mEndingSampleInclusive   = sampleClkOffsets[4];

    DrawMarkersDotsAndStates( 0, 4, AnalyzerResults::UpArrow, sampleDataState );
    mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );

	// decode type
    mRffeType = RFFEUtil::decodeRFFECmdFrame( (U8)(cmd & 0xFF) );
    switch ( mRffeType )
    {
    case RFFEAnalyzerResults::RffeTypeExtWrite:
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[8];

        DrawMarkersDotsAndStates( 4, 4, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeExByteCountField;
        frame.mData1                   = ( cmd & 0x0F );
	    frame.mStartingSampleInclusive = sampleClkOffsets[8] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 8, 4, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        count = RFFEUtil::byteCount( (U8)cmd );
        break;
    case RFFEAnalyzerResults::RffeTypeReserved: 
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 5, 13, AnalyzerResults::ErrorSquare, sampleDataState );
    	mResults->AddFrame( frame );
        break;
    case RFFEAnalyzerResults::RffeTypeExtRead:
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[8];

        DrawMarkersDotsAndStates( 5, 9, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeExByteCountField;
        frame.mData1                   = ( cmd & 0x0F );
	    frame.mStartingSampleInclusive = sampleClkOffsets[8] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 9, 13, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        count = RFFEUtil::byteCount( (U8)cmd );
        break;
    case RFFEAnalyzerResults::RffeTypeExtLongWrite:
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[9];

        DrawMarkersDotsAndStates( 5, 10, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeExLongByteCountField;
        frame.mData1                   = ( cmd & 0x07 );
	    frame.mStartingSampleInclusive = sampleClkOffsets[9] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 10, 13, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        count = RFFEUtil::byteCount( (U8)cmd );
        break;
    case RFFEAnalyzerResults::RffeTypeExtLongRead:
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[9];

        DrawMarkersDotsAndStates( 5, 10, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeExLongByteCountField;
        frame.mData1                   = ( cmd & 0x07 );
	    frame.mStartingSampleInclusive = sampleClkOffsets[9] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 10, 13, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        count = RFFEUtil::byteCount( (U8)cmd );
        break;
    case RFFEAnalyzerResults::RffeTypeNormalWrite:
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4];
	    frame.mEndingSampleInclusive   = sampleClkOffsets[7];

        DrawMarkersDotsAndStates( 4, 3, AnalyzerResults::UpArrow, sampleDataState );
        mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeShortAddressField;
        frame.mData1                   = ( cmd & 0x1F );
	    frame.mStartingSampleInclusive = sampleClkOffsets[7];
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 7, 5, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );
        break;
    case RFFEAnalyzerResults::RffeTypeNormalRead:
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[7];

        DrawMarkersDotsAndStates( 5, 8, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeShortAddressField;
        frame.mData1                   = ( cmd & 0x1F );
	    frame.mStartingSampleInclusive = sampleClkOffsets[7] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 8, 13, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );
        break;
    case RFFEAnalyzerResults::RffeTypeShortWrite:
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[5];

        DrawMarkersDotsAndStates( 5, 6, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeShortDataField;
        frame.mData1                   = ( cmd & 0x7F ) >> 7;
	    frame.mStartingSampleInclusive = sampleClkOffsets[5] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 6, 13, AnalyzerResults::UpArrow, sampleDataState );
    	mResults->AddFrame( frame );
        break;
    }

	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );

    return count;
}

void RFFEAnalyzer::FindParity()
{
    U64 sample_clk;
    U64 sample_clk_next;
    U64 sample_data;
    Frame frame;
    BitState bitstate;
    AnalyzerResults::MarkerType state;

    bitstate = GetNextBit( 0, &sample_clk, &sample_data );
    sample_clk_next = mSclk->GetSampleNumber();
    mSdata->AdvanceToAbsPosition( sample_clk_next );

    if ( bitstate == BIT_HIGH )
    {
        frame.mData1 = 1;
        state = AnalyzerResults::One;
    }
    else
    {
        frame.mData1 = 0;
        state = AnalyzerResults::Zero;
    }

    frame.mType                    = RFFEAnalyzerResults::RffeParityField;
	frame.mStartingSampleInclusive = sample_clk;
	frame.mEndingSampleInclusive   = sample_clk_next;

    mResults->AddMarker( sample_clk,
                            AnalyzerResults::UpArrow,
                            mSettings->mSclkChannel );
    mResults->AddMarker( sample_data,
                            state,
                            mSettings->mSdataChannel );
    mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
}

void RFFEAnalyzer::FindBusPark()
{
    U64 sample_clk1;
    U64 sample_clk2;
    U64 sample_clk3;
    Frame frame;

    // at rising edge of clk
    sample_clk1 = mSclk->GetSampleNumber();
    mSclk->AdvanceToNextEdge();
    // at falling edge of clk
    sample_clk2 = mSclk->GetSampleNumber();
    mSdata->AdvanceToAbsPosition( sample_clk2 );
    // look if next rising edge is in reach
    sample_clk3 =  sample_clk2 - sample_clk1; // delta
    if ( mSclk->WouldAdvancingCauseTransition( (U32)(sample_clk3 + 2) ) )
    {
        mSclk->AdvanceToNextEdge();
        sample_clk3 = mSclk->GetSampleNumber();
    }
    else
    {
        sample_clk3 = sample_clk2 + sample_clk3;
    }

    frame.mType                    = RFFEAnalyzerResults::RffeBusParkField;
	frame.mStartingSampleInclusive = sample_clk1;
	frame.mEndingSampleInclusive   = sample_clk3;

    mResults->AddMarker( sample_clk1,
                         AnalyzerResults::UpArrow,
                         mSettings->mSclkChannel );

    mResults->AddMarker( sample_clk2,
                         AnalyzerResults::Stop,
                         mSettings->mSdataChannel );

    mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
}

void RFFEAnalyzer::FindDataFrame()
{
	Frame frame;
    AnalyzerResults::MarkerType sampleDataState[16];

    U64 addr = GetByte(sampleDataState);

    // decode slave address
    frame.mType                    = RFFEAnalyzerResults::RffeDataField;
    frame.mData1                   = addr;
	frame.mStartingSampleInclusive = sampleClkOffsets[0];
	frame.mEndingSampleInclusive   = sampleClkOffsets[8];

    DrawMarkersDotsAndStates( 0, 8, AnalyzerResults::UpArrow, sampleDataState );
    mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );

    FindParity();
}

void RFFEAnalyzer::FindAddressFrame()
{
	Frame frame;
    AnalyzerResults::MarkerType sampleDataState[16];

    U64 addr = GetByte(sampleDataState);

    // decode slave address
    frame.mType                    = RFFEAnalyzerResults::RffeAddressField;
    frame.mData1                   = addr;
	frame.mStartingSampleInclusive = sampleClkOffsets[0];
	frame.mEndingSampleInclusive   = sampleClkOffsets[8];

    DrawMarkersDotsAndStates( 0, 8, AnalyzerResults::UpArrow, sampleDataState );
    mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );

    FindParity();
}

U64 RFFEAnalyzer::GetByte(AnalyzerResults::MarkerType *states)
{
    U64 data;
    U32 i;
    BitState state;
	DataBuilder data_builder;

    data_builder.Reset( &data, AnalyzerEnums::MsbFirst , 8 );

    // starting at rising edge of clk
    for( i=0; i < 8; i++ )
    {
        state = GetNextBit( i, sampleClkOffsets,  sampleDataOffsets );
        data_builder.AddBit( state );

        if ( mSdata->GetBitState() == BIT_HIGH )
            states[i] = AnalyzerResults::One;
        else
            states[i] = AnalyzerResults::Zero;
    }
    sampleClkOffsets[i] =  mSclk->GetSampleNumber();

    return data;
}

bool RFFEAnalyzer::NeedsRerun()
{
	return false;
}

U32 RFFEAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate, SimulationChannelDescriptor** simulation_channels )
{
	if( mSimulationInitilized == false )
	{
		mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
		mSimulationInitilized = true;
	}

	return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 RFFEAnalyzer::GetMinimumSampleRateHz()
{
	return 50000000;
}

const char* RFFEAnalyzer::GetAnalyzerName() const
{
	return "RFFEv1.0";
}

const char* GetAnalyzerName()
{
	return "RFFEv1.0";
}

Analyzer* CreateAnalyzer()
{
	return new RFFEAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
	delete analyzer;
}