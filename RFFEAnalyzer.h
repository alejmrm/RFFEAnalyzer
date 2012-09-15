#ifndef RFFE_ANALYZER_H
#define RFFE_ANALYZER_H

#include <Analyzer.h>
#include "RFFEAnalyzerResults.h"
#include "RFFESimulationDataGenerator.h"

#pragma warning( push )
//warning C4275: non dll-interface class 'Analyzer2' used 
//               as base for dll-interface class 'RFFEAnalyzer'
#pragma warning( disable : 4275 )

class RFFEAnalyzerSettings;
class ANALYZER_EXPORT RFFEAnalyzer : public Analyzer2
{
public:
	RFFEAnalyzer();
	virtual ~RFFEAnalyzer();
	virtual void SetupResults();
	virtual void WorkerThread();

	virtual U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
	virtual U32 GetMinimumSampleRateHz();

	virtual const char* GetAnalyzerName() const;
	virtual bool NeedsRerun();

#pragma warning( push )
    //warning C4251: 'RFFEAnalyzer::<...>' : class <...> needs to have dll-interface
    //               to be used by clients of class
#pragma warning( disable : 4251 )

protected: //vars
	std::auto_ptr< RFFEAnalyzerSettings > mSettings;
	std::auto_ptr< RFFEAnalyzerResults > mResults;
	AnalyzerChannelData* mSclk;
	AnalyzerChannelData* mSdata;

	RFFESimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

	//Serial analysis vars:
	U32 mSampleRateHz;
    U32 mStartOfStopBitOffset;
	U32 mEndOfStopBitOffset;
    RFFEAnalyzerResults::RffeTypeFieldType mRffeType;

protected: // functions
	void AdvanceToBeginningStartBit();
    void FindStartSeqCondition();
    void FindCommand();
    void FindParity();
    void FindDataFrames();
    void FindBusPark();
    void DrawMarkersDotsAndStates( U32 start,
                                   U32 end,
                                   AnalyzerResults::MarkerType type,
                                   AnalyzerResults::MarkerType *states);

private:
    U64 sampleClkOffsets[24];
    U64 sampleDataOffsets[24];

#pragma warning( pop )
};
#pragma warning( pop )

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //RFFE_ANALYZER_H
