#include "RFFEAnalyzer.h"
#include "RFFEAnalyzerSettings.h"
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
	mSampleRateHz = GetSampleRate();

	mSdata = GetAnalyzerChannelData( mSettings->mSdataChannel );
	mSclk  = GetAnalyzerChannelData( mSettings->mSclkChannel );

	U32 samples_per_bit = mSampleRateHz / mSettings->mBitRate;
	U32 samples_to_first_center_of_first_data_bit = U32( 1.5 * double( mSampleRateHz ) / double( mSettings->mBitRate ) );

	for( ; ; )
	{
        FindStartSeqCondition();
        FindCommand();
        FindParity();
        FindDataFrames();
        //FindBusPark();
        CheckIfThreadShouldExit();
	}
}

void RFFEAnalyzer::AdvanceToBeginningStartBit()
{
	for( ; ; )
	{
		mSdata->AdvanceToNextEdge();

		if( mSdata->GetBitState() == BIT_HIGH )
		{
			mSclk->AdvanceToAbsPosition( mSdata->GetSampleNumber() );
			if( mSclk->GetBitState() == BIT_LOW )
				break;
		}	
	}
}

void RFFEAnalyzer::FindStartSeqCondition()
{
    U64 sample_up;
    U64 sample_dn;
    U64 sample_next;
    U64 pulse_1, pulse_2;
    bool is_toggle;

    for ( ; ; )
    {
        mSclk->AdvanceToAbsPosition( mSdata->GetSampleNumber() );
        if ( mSdata->GetBitState() == BIT_LOW )
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

        is_toggle = mSclk->WouldAdvancingToAbsPositionCauseTransition( sample_dn );
        if( is_toggle ) continue; // error: found clk toggling

        // look for idle in clk & data signals
        mSclk->AdvanceToAbsPosition( sample_dn );
        sample_next = mSclk->GetSampleOfNextEdge();
        pulse_2     = sample_next - sample_dn;

        if ( _abs64(pulse_2 - pulse_1) > 2 ) continue; // error: idle period too shoot

        // at rising edge of clk
        mSclk->AdvanceToNextEdge();
        mSdata->AdvanceToAbsPosition( sample_next );

		// save ssc signal. 
		Frame frame;
        frame.mType                    = RFFEAnalyzerResults::RffeSSCField;
		frame.mStartingSampleInclusive = sample_up;
		frame.mEndingSampleInclusive   = sample_next;

        mResults->AddMarker( sample_up, AnalyzerResults::Start, mSettings->mSdataChannel );
		mResults->AddFrame( frame );
		mResults->CommitResults();

		ReportProgress( frame.mEndingSampleInclusive );
        break;
    }
}

void RFFEAnalyzer::DrawMarkersDotsAndStates( U32 start,
                                             U32 end,
                                             AnalyzerResults::MarkerType type,
                                             AnalyzerResults::MarkerType *states)
{
    for (U32 i=start; i < end; i++ )
    {
        mResults->AddMarker( sampleClkOffsets[i],
                             type,
                             mSettings->mSdataChannel );
        mResults->AddMarker( sampleDataOffsets[i],
                             states[i],
                             mSettings->mSdataChannel );
    }
}

void RFFEAnalyzer::FindCommand()
{
    U64 cmd;
    U32 i;
	DataBuilder cmd_builder;
    AnalyzerResults::MarkerType sampleDataState[16];

    cmd_builder.Reset( &cmd, AnalyzerEnums::MsbFirst , 12 );

    // starting at rising edge of clk
    for( i=0; i < 12; i++ )
    {
        sampleClkOffsets[i] =  mSclk->GetSampleNumber();
        // falling edge of sclk
        mSclk->AdvanceToNextEdge();
        sampleDataOffsets[i] =  mSclk->GetSampleNumber();

        mSdata->AdvanceToAbsPosition( mSclk->GetSampleNumber() );
        // sample data
        cmd_builder.AddBit( mSdata->GetBitState() );

        if ( mSdata->GetBitState() == BIT_HIGH )
            sampleDataState[i] = AnalyzerResults::One;
        else
            sampleDataState[i] = AnalyzerResults::Zero;

        mSclk->AdvanceToNextEdge();
    }
    sampleClkOffsets[i] =  mSclk->GetSampleNumber();
    mSdata->AdvanceToAbsPosition( mSclk->GetSampleNumber() );

	Frame frame;
    // decode slave address
    frame.mType                    = RFFEAnalyzerResults::RffeSAField;
    frame.mData1                   = ( cmd & 0xF00 ) >> 8;
	frame.mStartingSampleInclusive = sampleClkOffsets[0];
	frame.mEndingSampleInclusive   = sampleClkOffsets[4];

    DrawMarkersDotsAndStates( 0, 5, AnalyzerResults::Dot, sampleDataState );
    mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );

	// decode type
    cmd = cmd & 0xFF;
    if ( cmd  < 0x10 )
    {
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType = RFFEAnalyzerResults::RffeTypeExtWrite;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[8];

        DrawMarkersDotsAndStates( 5, 9, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeExByteCountField;
        frame.mData1                   = ( cmd & 0x0F );
	    frame.mStartingSampleInclusive = sampleClkOffsets[8] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 9, 13, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );
    }
    else if ( (cmd >= 0x10) && (cmd < 0x20) )
    {
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType = RFFEAnalyzerResults::RffeTypeReserved;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 5, 13, AnalyzerResults::ErrorSquare, sampleDataState );
    	mResults->AddFrame( frame );
    }
    else if ( (cmd >= 0x20) && (cmd < 0x30) )
    {
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType = RFFEAnalyzerResults::RffeTypeExtRead;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[8];

        DrawMarkersDotsAndStates( 5, 9, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeExByteCountField;
        frame.mData1                   = ( cmd & 0x0F );
	    frame.mStartingSampleInclusive = sampleClkOffsets[8] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 9, 13, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );
    }
    else if ( (cmd >= 0x30) && (cmd < 0x38) )
    {
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType = RFFEAnalyzerResults::RffeTypeExtLongWrite;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[9];

        DrawMarkersDotsAndStates( 5, 10, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeExLongByteCountField;
        frame.mData1                   = ( cmd & 0x07 );
	    frame.mStartingSampleInclusive = sampleClkOffsets[9] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 10, 13, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );
    }
    else if ( (cmd >= 0x38) && (cmd < 0x40) )
    {
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType = RFFEAnalyzerResults::RffeTypeExtLongRead;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[9];

        DrawMarkersDotsAndStates( 5, 10, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeExLongByteCountField;
        frame.mData1                   = ( cmd & 0x07 );
	    frame.mStartingSampleInclusive = sampleClkOffsets[9] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 10, 13, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );
    }
    else if ( (cmd >= 0x40) && (cmd < 0x60) )
    {
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType = RFFEAnalyzerResults::RffeTypeNormalWrite;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[7];

        DrawMarkersDotsAndStates( 5, 8, AnalyzerResults::Dot, sampleDataState );
        mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeShortAddressField;
        frame.mData1                   = ( cmd & 0x1F );
	    frame.mStartingSampleInclusive = sampleClkOffsets[7] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 8, 13, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );
    }
    else if ( (cmd >= 0x60) && (cmd < 0x80) )
    {
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType = RFFEAnalyzerResults::RffeTypeNormalRead;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[7];

        DrawMarkersDotsAndStates( 5, 8, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeShortAddressField;
        frame.mData1                   = ( cmd & 0x1F );
	    frame.mStartingSampleInclusive = sampleClkOffsets[7] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 8, 13, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );
    }
    else
    {
        frame.mType                    = RFFEAnalyzerResults::RffeTypeField;
        frame.mData1                   = mRffeType = RFFEAnalyzerResults::RffeTypeShortWrite;
	    frame.mStartingSampleInclusive = sampleClkOffsets[4] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[5];

        DrawMarkersDotsAndStates( 5, 6, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );

        frame.mType                    = RFFEAnalyzerResults::RffeShortDataField;
        frame.mData1                   = ( cmd & 0x7F ) >> 7;
	    frame.mStartingSampleInclusive = sampleClkOffsets[5] + 1;
	    frame.mEndingSampleInclusive   = sampleClkOffsets[12];

        DrawMarkersDotsAndStates( 6, 13, AnalyzerResults::Dot, sampleDataState );
    	mResults->AddFrame( frame );
    }

	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
}

void RFFEAnalyzer::FindParity()
{
    U64 sample_clk;
    U64 sample_data;
    Frame frame;
    AnalyzerResults::MarkerType state;

    sample_clk = mSclk->GetSampleNumber() + 1;
    mSclk->AdvanceToNextEdge();

    sample_data = mSclk->GetSampleNumber();
    mSdata->AdvanceToAbsPosition( sample_data );
    if ( mSdata->GetBitState() == BIT_HIGH )
    {
        frame.mData1 = 1;
        state = AnalyzerResults::One;
    }
    else
    {
        frame.mData1 = 0;
        state = AnalyzerResults::Zero;
    }
    mSclk->AdvanceToNextEdge();
    mSdata->AdvanceToAbsPosition( mSclk->GetSampleNumber() );

    frame.mType                    = RFFEAnalyzerResults::RffeParityField;
	frame.mStartingSampleInclusive = sample_clk;
	frame.mEndingSampleInclusive   = mSclk->GetSampleNumber();

    mResults->AddMarker( sample_clk,
                            AnalyzerResults::Dot,
                            mSettings->mSdataChannel );
    mResults->AddMarker( sample_data,
                            state,
                            mSettings->mSdataChannel );
    mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
}

void RFFEAnalyzer::FindBusPark()
{
    U64 sample_clk1, sample_clk2;
    Frame frame;

    sample_clk1 = mSclk->GetSampleNumber();
    mSclk->AdvanceToNextEdge();
    sample_clk2 = mSclk->GetSampleNumber();
    mSdata->AdvanceToAbsPosition( mSclk->GetSampleNumber() );

    if ( mSdata->GetBitState() == BIT_LOW )
    {
        frame.mType                    = RFFEAnalyzerResults::RffeBusParkField;
	    frame.mStartingSampleInclusive = sample_clk1;
	    frame.mEndingSampleInclusive   = (sample_clk2 - sample_clk1) * 2;

        mResults->AddMarker( sample_clk1,
                             AnalyzerResults::Stop,
                             mSettings->mSdataChannel );
    }
    else
    {
        frame.mType                    = RFFEAnalyzerResults::RffeBusParkField;
	    frame.mStartingSampleInclusive = sample_clk1;
	    frame.mEndingSampleInclusive   = (sample_clk2 - sample_clk1) * 2;

        mResults->AddMarker( sample_clk1,
                             AnalyzerResults::ErrorDot,
                             mSettings->mSdataChannel );
    }
    mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
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
	return mSettings->mBitRate * 4;
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