#ifndef MSG_DEF_H
#define MSG_DEF_H

#include <string>

// Messages received from the simulation process
enum class MsgFromSim { FinishedShmAccess, ClosedShm };


// Messages, posted to the simulator thread and process
struct MsgToSim
{
	enum MsgType { InitSim, UpdateDimensions, UpdateGrid, CloseShm, Exit, SimulatorCmd, StartProcess, Empty } type;

	// Needed to make type polymorphic
	MsgToSim(MsgType t = Empty) : type(t){};
	virtual ~MsgToSim(){};
};

// Containing a string
struct StrMsg : MsgToSim
{
	std::string str;
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

// Messages, posted to the renderer thread
struct MsgToRenderer
{
	enum MsgType { Empty } type;
};





#endif