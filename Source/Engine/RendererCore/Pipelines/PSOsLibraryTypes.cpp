#include "PSOsLibraryTypes.h"
#include "PSOsLibrary.h"

namespace spt::rdr
{

void RegisterPSO(PSOPrecacheFunction callable)
{
	PSOsLibrary::GetInstance().RegisterPSO(callable);
}

} // spt::rdr