#ifndef RFFE_SIMULATION_DATA_GENERATOR
#define RFFE_SIMULATION_DATA_GENERATOR

#include <AnalyzerHelpers.h>
#include <SimulationChannelDescriptor.h>
#include <string>
class RFFEAnalyzerSettings;

class RFFESimulationDataGenerator
{
public:
	RFFESimulationDataGenerator();
	~RFFESimulationDataGenerator();

	void Initialize( U32 simulation_sample_rate, RFFEAnalyzerSettings* settings );
	U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );

protected:
	RFFEAnalyzerSettings* mSettings;
	U32 mSimulationSampleRateHz;
    U32 mParityCounter;

protected: // RFFE specific functions
	void CreateRffeTransaction();
	void CreateStart();
    void CreateSlaveAddress(U8 addr);
    void CreateByte(U8 cmd);
    void CreateParity();
    void CreateBusPark();
    void CreateDataFrame(U32 decode);

protected: //RFFE specific vars
	ClockGenerator mClockGenerator;
	SimulationChannelDescriptorGroup mRffeSimulationChannels;
	SimulationChannelDescriptor* mSclk;
	SimulationChannelDescriptor* mSdata;
};
#endif //RFFE_SIMULATION_DATA_GENERATOR