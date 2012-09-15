#include "RFFESimulationDataGenerator.h"
#include "RFFEAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

RFFESimulationDataGenerator::RFFESimulationDataGenerator()
{
}

RFFESimulationDataGenerator::~RFFESimulationDataGenerator()
{
}

void RFFESimulationDataGenerator::Initialize( U32 simulation_sample_rate, RFFEAnalyzerSettings* settings )
{
	mSimulationSampleRateHz = simulation_sample_rate;
	mSettings = settings;

    mClockGenerator.Init( simulation_sample_rate / 10, simulation_sample_rate );

    if( settings->mSclkChannel != UNDEFINED_CHANNEL )
		mSclk = mRffeSimulationChannels.Add( settings->mSclkChannel,
                                              mSimulationSampleRateHz,
                                              BIT_LOW );
	else
		mSclk = NULL;

    if( settings->mSdataChannel != UNDEFINED_CHANNEL )
		mSdata = mRffeSimulationChannels.Add( settings->mSdataChannel,
                                              mSimulationSampleRateHz,
                                              BIT_LOW );
	else
		mSdata = NULL;

    //insert 10 bit-periods of idle
	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 10.0 ) );

	mParityCounter = 0;
}

U32 RFFESimulationDataGenerator::GenerateSimulationData( U64 largest_sample_requested, 
                                                         U32 sample_rate, 
                                                         SimulationChannelDescriptor** simulation_channels )
{
	U64 adjusted_largest_sample_requested = AnalyzerHelpers::AdjustSimulationTargetSample( largest_sample_requested,
                                                                                           sample_rate,
                                                                                           mSimulationSampleRateHz );

	while( mSclk->GetCurrentSampleNumber() < adjusted_largest_sample_requested )
	{
		CreateRffeTransaction();
        
		mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 20.0 ) );
    }

	*simulation_channels = mRffeSimulationChannels.GetArray();
	return mRffeSimulationChannels.GetCount();
}

void RFFESimulationDataGenerator::CreateRffeTransaction()
{
	U32 samples_per_bit = mSimulationSampleRateHz / mSettings->mBitRate;
    U8 cmd;
    U8 cmd_frames[] = 
    {
        0x00, 0x07, 0x0F,
        0x10, 0x1B, 0x1F,
        0x20, 0x23, 0x2F,
        0x30, 0x36, 0x37,
        0x38, 0x3D, 0x3F,
        0x40, 0x43, 0x55, 0x5F,
        0x60, 0x64, 0x78, 0x7F,
        0x80, 0x91, 0xA3, 0xBC, 0xC1, 0xD9, 0xEF, 0xF2, 0xFF
    };
    U8 sa_addrs[] =
    {
        0x5, 0x7, 0x8, 0x2
    };
    
    for( U32 adr=0 ; adr < 2/*sizeof(sa_addrs)/sizeof(sa_addrs[0])*/ ; adr++ )
    {
        for ( U32 cmd_idx=0 ; cmd_idx < sizeof(cmd_frames)/sizeof(cmd_frames[0]) ; cmd_idx++ )
        {
            CreateStart();
            CreateSlaveAddress( sa_addrs[adr] );
            cmd = cmd_frames[cmd_idx];
            CreateByte( cmd );
            CreateParity();
            CreateDataFrame( cmd );
        }
    }
}

void RFFESimulationDataGenerator::CreateStart()
{
    if ( mSclk->GetCurrentBitState() == BIT_HIGH )
    {
		mSclk->Transition();
    }

	if( mSdata->GetCurrentBitState() == BIT_HIGH )
    {
		mSdata->Transition();
    }

	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 2.0 ) );

    // sdata pulse for 1-clock cycle
    mSdata->Transition();
	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 2.0 ) );
    mSdata->Transition();
    // sdata and sclk state low for 1-clock cycle
	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 2.0 ) );

    mParityCounter = 0;
}

void RFFESimulationDataGenerator::CreateSlaveAddress(U8 addr )
{
    U8 address = addr & 0x0F;
    BitExtractor adr_bits( address, AnalyzerEnums::MsbFirst, 4 );

	for( U32 i=0; i< 4; i++ )
	{
		mSclk->Transition();
		mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );

		mSdata->TransitionIfNeeded( adr_bits.GetNextBit() );

        if( mSdata->GetCurrentBitState() == BIT_HIGH ) 
            mParityCounter++;

		mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );
		mSclk->Transition();

    	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 1.0 ) );
	}
}

void RFFESimulationDataGenerator::CreateByte(U8 cmd)
{
    BitExtractor cmd_bits( cmd, AnalyzerEnums::MsbFirst, 8 );

	for( U32 i=0; i< 8; i++ )
	{
		mSclk->Transition();
		mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );

		mSdata->TransitionIfNeeded( cmd_bits.GetNextBit() );

        if( mSdata->GetCurrentBitState() == BIT_HIGH ) 
            mParityCounter++;

		mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );
		mSclk->Transition();

    	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 1.0 ) );
	}
}

void RFFESimulationDataGenerator::CreateParity()
{
	mSclk->Transition();
	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );

    if( AnalyzerHelpers::IsEven(mParityCounter) )
    {
		mSdata->TransitionIfNeeded( BIT_HIGH );
    }

	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );
	mSclk->Transition();

    mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 1.0 ) );
}

void RFFESimulationDataGenerator::CreateBusPark()
{
	mSclk->Transition();
	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );

	mSdata->TransitionIfNeeded( BIT_LOW );

	mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );
	mSclk->Transition();

    mRffeSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 1.0 ) );

    mParityCounter = 0;
}

void RFFESimulationDataGenerator::CreateDataFrame(U32 decode)
{
    if ( decode  < 0x10 )
    {
        CreateByte( 0xBC );
        CreateParity();
        for( U32 i=0; i <= (decode & 0x0F); i++ )
        {
            CreateByte( 0x40 + i );
            CreateParity();
        }
        CreateBusPark();
    }
    else if ( (decode >= 0x10) && (decode < 0x20) )
    {
    }
    else if ( (decode >= 0x20) && (decode < 0x30) )
    {
        CreateByte( 0x9A );
        CreateParity();
        CreateBusPark();
        for( U32 i=0; i <= (decode & 0x0F); i++ )
        {
            CreateByte( 0x30 + i );
            CreateParity();
        }
        CreateBusPark();
    }
    else if ( (decode >= 0x30) && (decode < 0x38) )
    {
        CreateByte( 0x56 );
        CreateParity();
        CreateByte( 0x78 );
        CreateParity();
        for( U32 i=0; i <= (decode & 0x07); i++ )
        {
            CreateByte( 0x10 + i );
            CreateParity();
        }
        CreateBusPark();
    }
    else if ( (decode >= 0x38) && (decode < 0x40) )
    {
        CreateByte( 0x12 );
        CreateParity();
        CreateByte( 0x34 );
        CreateParity();
        CreateBusPark();
        for( U32 i=0; i <= (decode & 0x07); i++ )
        {
            CreateByte( 0x10 + i );
            CreateParity();
        }
        CreateBusPark();
    }
    else if ( (decode >= 0x40) && (decode < 0x60) )
    {
        CreateByte( 0x81 );
        CreateParity();
        CreateBusPark();
    }
    else if ( (decode >= 0x60) && (decode < 0x80) )
    {
        CreateBusPark();
        CreateByte( 0x2C );
        CreateParity();
        CreateBusPark();
    }
    else
    {
        CreateBusPark();
    }
}
