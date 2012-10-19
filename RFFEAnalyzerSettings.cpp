#include "RFFEAnalyzerSettings.h"
#include <AnalyzerHelpers.h>


RFFEAnalyzerSettings::RFFEAnalyzerSettings()
:	mSclkChannel( UNDEFINED_CHANNEL ),
    mSdataChannel( UNDEFINED_CHANNEL )
{
	mSclkChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
	mSclkChannelInterface->SetTitleAndTooltip( "SCLK", "Specify the SCLK Signal(RFFEv1.0)" );
	mSclkChannelInterface->SetChannel( mSclkChannel );
	AddInterface( mSclkChannelInterface.get() );

	mSdataChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
	mSdataChannelInterface->SetTitleAndTooltip( "SDATA", "Specify the SDATA Signal(RFFEv1.0)" );
	mSdataChannelInterface->SetChannel( mSdataChannel );
	AddInterface( mSdataChannelInterface.get() );

	mShowParityInReportInterface.reset( new AnalyzerSettingInterfaceBool() );
	mShowParityInReportInterface->SetTitleAndTooltip("Show Parity in Report?",
		"Check if you want party information in the exported file" );
	AddInterface( mShowParityInReportInterface.get() );

	AddExportOption( 0, "Export as csv/text file" );
	AddExportExtension( 0, "csv", "csv" );
	AddExportExtension( 0, "text", "txt" );

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
	mShowParityInReport = mShowParityInReportInterface->GetValue();

	ClearChannels();
	AddChannel( mSclkChannel, "SCLK", true );
	AddChannel( mSdataChannel, "SDATA", true );

	return true;
}

void RFFEAnalyzerSettings::UpdateInterfacesFromSettings()
{
	mSclkChannelInterface->SetChannel( mSclkChannel );
	mSdataChannelInterface->SetChannel( mSdataChannel );
	mShowParityInReportInterface->SetValue(mShowParityInReport);
}

void RFFEAnalyzerSettings::LoadSettings( const char* settings )
{
	SimpleArchive text_archive;
	text_archive.SetString( settings );

	text_archive >> mSclkChannel;
	text_archive >> mSdataChannel;
	text_archive >> mShowParityInReport;

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
	text_archive << mShowParityInReport;

	return SetReturnString( text_archive.GetString() );
}
