#include "cids.h"
#include "controller.h"
#include "processor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg::Vst;
using namespace PhatBooty;

BEGIN_FACTORY_DEF (stringCompanyName, stringCompanyWeb, stringCompanyEmail)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kProcessorUID),
	            PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            stringPluginName,
	            Vst::kDistributable,
	            PhatBootyVST3Category,
	            FULL_VERSION_STR,
	            kVstVersionString,
	            Processor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kControllerUID),
	            PClassInfo::kManyInstances,
	            kVstComponentControllerClass,
	            stringPluginName "Controller",
	            0,
	            "",
	            FULL_VERSION_STR,
	            kVstVersionString,
	            Controller::createInstance)

END_FACTORY
