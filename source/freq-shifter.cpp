#include "clap/all.h"
#include "clap/helpers/plugin.hh"
#include "clap/helpers/plugin.hxx"

#include "./hilbert.h"

using PluginHelper = clap::helpers::Plugin<clap::helpers::MisbehaviourHandler::Ignore, clap::helpers::CheckingLevel::Maximal>;

class FreqShifter : public PluginHelper {
public:
	static const char * descriptorFeatures[];
	static const clap_plugin_descriptor descriptor;
	
	static const clap_plugin * create(const clap_host *host) {
		auto *plugin = new FreqShifter(host);
		return plugin->clapPlugin();
	}

	FreqShifter(const clap_host* host) : PluginHelper(&descriptor, host) {}

};

const char * FreqShifter::descriptorFeatures[] = {"audio-effect", nullptr};
const clap_plugin_descriptor FreqShifter::descriptor = {
	.clap_version = CLAP_VERSION,
	.id = "uk.co.signalsmith.wclap.demo.freq-shifter",
	.name = "Frequency Shifter",
	.vendor = "Signalsmith Audio",
	.url = "",
	.manual_url = "",
	.support_url = "",
	.version = "1.0.0",
	.features = descriptorFeatures
};

//------------- Plugin factory -------------

static uint32_t clap_get_plugin_count(const clap_plugin_factory* /*factory*/) {
	return 1;
}
static const clap_plugin_descriptor* clap_get_plugin_descriptor(const clap_plugin_factory* /*factory*/, uint32_t index) {
	if (index == 0) return &FreqShifter::descriptor;
	return nullptr;
}
static const clap_plugin* clap_create_plugin(const clap_plugin_factory *, const clap_host *host, const char *id) {
	if (!std::strcmp(id, FreqShifter::descriptor.id)) return FreqShifter::create(host);
	return nullptr;
}

const static struct clap_plugin_factory pluginFactory = {
	.get_plugin_count=clap_get_plugin_count,
	.get_plugin_descriptor=clap_get_plugin_descriptor,
	.create_plugin=clap_create_plugin
};

//------------- Module entry point -------------

static bool clap_init(const char* /*pluginPath*/) {
	return true;
}
static void clap_deinit() {}
static const void* clap_get_factory(const char* factoryId) {
	if (!strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID)) {
		return &pluginFactory;
	}
	return nullptr;
}

extern "C" {
	const CLAP_EXPORT clap_plugin_entry clap_entry{
		.clap_version = CLAP_VERSION,
		.init = clap_init,
		.deinit = clap_deinit,
		.get_factory = clap_get_factory
	};
}
