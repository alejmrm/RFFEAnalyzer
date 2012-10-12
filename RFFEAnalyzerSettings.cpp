#include "RFFEAnalyzerSettings.h"
#include <AnalyzerHelpers.h>


RFFEAnalyzerSettings::RFFEAnalyzerSettings()
:	mSclkChannel( UNDEFINED_CHANNEL ),
    mSdataChannel( UNDEFINED_CHANNEL )
{
	mSclkChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
	mSclkChannelInterface->SetTitleAndTooltip( "SCLK", "Specify the SCLK Signal(RFFEv1.0)" );
	mSclkChannelInterface->SetChannel( mSclkChannel );

	mSdataChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
	mSdataChannelInterface->SetTitleAndTooltip( "SDATA", "Specify the SDATA Signal(RFFEv1.0)" );
	mSdataChannelInterface->SetChannel( mSdataChannel );

	AddInterface( mSclkChannelInterface.get() );
	AddInterface( mSdataChannelInterface.get() );

	AddExportOption( 0, "Export as text/csv file" );
	AddExportExtension( 0, "text", "txt" );
	AddExportExtension( 0, "csv", "csv" );

	ClearChannels();
	AddChannel( mSclkChannel, "SCLK", false );
	AddChannel( mSdataChannel, "SDATA", false );
}

RFFEAnalyzerSettings::~RFFEAnalyzerSettings()
{
}

bool RFFEAnalyzerSettings::SetSettingsFromInterfaces()
{
	mSclkChannel = mSclkChannelInterface->GetChannel();
	mSdataChannel = mSdataChannelInterface->GetChannel();

	ClearChannels();
	AddChannel( mSclkChannel, "SCLK", true );
	AddChannel( mSdataChannel, "SDATA", true );

	return true;
}

void RFFEAnalyzerSettings::UpdateInterfacesFromSettings()
{
	mSclkChannelInterface->SetChannel( mSclkChannel );
	mSdataChannelInterface->SetChannel( mSdataChannel );
}

void RFFEAnalyzerSettings::LoadSettings( const char* settings )
{
	SimpleArchive text_archive;
	text_archive.SetString( settings );

	text_archive >> mSclkChannel;
	text_archive >> mSdataChannel;

	ClearChannels();
	AddChannel( mSclkChannel, "SCLK", true );
	AddChannel( mSdataChannel, "SDATA", true );

	UpdateInterfacesFromSettings();
}

const char* RFFEAnalyzerSettings::SaveSettings()
{
	SimpleArchive text_archive;

	text_archive << mSclkChannel;
	text_archive << mSdataChannel;

	return SetReturnString( text_archive.GetString() );
}
