#ifndef MSG_DEF_H
#define MSG_DEF_H

#include <string>

// Messages received from the simulation process
enum class MsgFromSimProc { Initialized, FinishedVoxelGridAccess, FinishedVelocityAccess, ClosedShm };

enum class MsgToSimProc { InitSim, UpdateDimensions, UpdateGrid, FillVelocity, CloseShm, Exit };


// Messages, posted to the simulator thread and process
struct MsgToSim
{
	enum MsgType { InitSim, UpdateDimensions, UpdateGrid, FillVelocity, Exit, SimulatorCmd, StartProcess, Empty } type;

	// Needed to make type polymorphic
	MsgToSim(MsgType t = Empty) : type(t){};
	virtual ~MsgToSim(){};
};

// Containing dimensions
struct DimMsg : MsgToSim
{
	struct Resolution
	{
		int x;
		int y;
		int z;
	} res;

	struct VoxelSize
	{
		float x;
		float y;
		float z;
	} vs;
};

// Additionally containing a string
struct StrMsg : DimMsg
{
	std::string str;
};

// Messages, posted to the renderer thread
struct MsgToRenderer
{
	enum MsgType { UpdateVelocity, Empty } type;
};





#endif