#include "clap/all.h"
#include "clap/helpers/plugin.hh"
#include "clap/helpers/plugin.hxx"

#include "cbor-walker.h"
#include "hilbert.h"

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

	double shiftHz = 0;
	const clap_param_info shiftHzInfo{
		.id=0xB7C3E57F,
		.flags=CLAP_PARAM_IS_AUTOMATABLE,
		.min_value=-1000,
		.max_value=1000,
		.default_value=0
	};

	// Parameter stuff
	bool implementsParams() const noexcept override {
		return true;
	}
	uint32_t paramsCount() const noexcept override {
		return 1;
	}
	bool paramsInfo(uint32_t index, clap_param_info *info) const noexcept override {
		if (index > 0) return false;
		*info = shiftHzInfo;
		return true;
      }
      bool paramsValue(clap_id paramId, double *value) noexcept override {
		if (paramId == shiftHzInfo.id) {
			*value = shiftHz;
			return true;
		}
		return false;
      }
      bool paramsValueToText(clap_id paramId, double value, char *display, uint32_t size) noexcept override {
		return false;
      }
      bool paramsTextToValue(clap_id paramId, const char *display, double *value) noexcept override {
		return false;
      }
      void paramsFlush(const clap_input_events *inEvents, const clap_output_events *out) noexcept override {
		uint32_t numInputEvents = inEvents->size(inEvents);
		for (uint32_t i = 0; i < numInputEvents; ++i) {
			handleEvent(inEvents->get(inEvents, 0));
		}
	}
	
	bool implementsAudioPorts() const noexcept override {
		return true;
	}
	uint32_t audioPortsCount(bool isInput) const noexcept override {
		return 1;
	}
	bool audioPortsInfo(uint32_t index, bool isInput, clap_audio_port_info *info) const noexcept override {
		if (index > 0) return false;
		*info = {
			.id=(isInput ? 0xEA97800D : 0x5A88FC32),
			.name={'m', 'a', 'i', 'n'}, // gets zero-padded
			.flags=CLAP_AUDIO_PORT_IS_MAIN,
			.channel_count=2,
			.port_type=CLAP_PORT_STEREO,
			.in_place_pair=CLAP_INVALID_ID
		};
		return true;
      }
      
      bool implementsState() const noexcept override {
		return true;
	}
      bool stateSave(const clap_ostream *stream) noexcept override {
		auto cbor = getCbor();
		cbor.openMap();
		cbor.addUtf8("shiftHz");
		cbor.addFloat(shiftHz);
		cbor.close();
		
		// Send CBOR to stream
		size_t index = 0;
		while (index < cborBuffer.size()) {
			int64_t bytesWritten = stream->write(stream, cborBuffer.data() + index, cborBuffer.size() - index);
			if (bytesWritten <= 0) return false;
			index += bytesWritten;
		}
		cborBuffer.resize(0);
		return true;
	}
      bool stateLoad(const clap_istream *stream) noexcept override {
		// Re-use the same buffer, but this time we're reading from it
		cborBuffer.resize(0);
		while (cborBuffer.size() < cborBuffer.capacity() - 1024) {
			size_t index = cborBuffer.size();
			cborBuffer.resize(index + 1024);
			int64_t bytesRead = stream->read(stream, cborBuffer.data() + index, 1024);
			if (bytesRead == 0) break; // end of stream
			if (bytesRead < 0) return false; // error
			index += bytesRead;
			cborBuffer.resize(index);
		}
		
		using Cbor = signalsmith::cbor::CborWalker;
		Cbor cbor(cborBuffer);
		if (!cbor.isMap()) return false;
		cbor.forEachPair([&](Cbor key, Cbor value){
			if (key.utf8View() == "shiftHz" && value.isNumber()) {
				shiftHz = value;
			}
		});
		return !cbor.error();
	}
	
	void handleEvent(const clap_event_header* event) {
		if (event->space_id != CLAP_CORE_EVENT_SPACE_ID) return;

		if (event->type == CLAP_EVENT_PARAM_VALUE) {
			const auto* paramValueEvent = reinterpret_cast<const clap_event_param_value*>(event);
			if (paramValueEvent->param_id == shiftHzInfo.id) {
				shiftHz = paramValueEvent->value;
				_host.stateMarkDirty();
				
				auto cbor = getCbor();
				cbor.openMap();
				cbor.addUtf8("shiftHz");
				cbor.addFloat(shiftHz);
				cbor.close();
				sendCborToWeb();
			}
		}
	}
	
	bool activate(double sRate, uint32_t, uint32_t) noexcept override {
		sampleRate = float(sRate);
		hilbert = {sampleRate, 2};
		return true;
      }
      
      void reset() noexcept override {
		hilbert.reset();
		shiftPhase = 0;
	}
	
	clap_process_status process(const clap_process *process) noexcept override {
		// a hack ideally we use the position within the block
		paramsFlush(process->in_events, process->out_events);

		const uint32_t blockLength = process->frames_count;
		if (process->audio_inputs_count != 1) return CLAP_PROCESS_ERROR;
		if (process->audio_outputs_count != 1) return CLAP_PROCESS_ERROR;
		float ** const inputs = process->audio_inputs[0].data32;
		float **outputs = process->audio_outputs[0].data32;
		
		float phaseStep = shiftHz/sampleRate;
		for (uint32_t i = 0; i < blockLength; ++i) {
			for (int c = 0; c < 2; ++c) {
				float x = inputs[c][i];
				auto cx = hilbert(x, c);
				cx *= std::polar(1.0f, shiftPhase*float(2*M_PI));
				outputs[c][i] = cx.real();
			}
			shiftPhase += phaseStep;
		}
		shiftPhase -= std::floor(shiftPhase);

		return CLAP_PROCESS_CONTINUE_IF_NOT_QUIET;
      }

	std::vector<unsigned char> cborBuffer = std::vector<unsigned char>(16384);
	signalsmith::cbor::CborWriter getCbor() {
		cborBuffer.resize(0);
		return {cborBuffer};
	}
	const clap_host_web *hostWeb;
	void sendCborToWeb() {
		if (hostWeb) {
			hostWeb->send(_host.host(), (void *)cborBuffer.data(), cborBuffer.size());
		}
	}
	bool webReceive(const void* buffer, uint32_t size) {
		using Cbor = signalsmith::cbor::TaggedCborWalker;
		Cbor cbor{(unsigned char *)buffer, (unsigned char *)buffer + size};
		if (cbor.isUtf8() && cbor.utf8View() == "open") {
			// Send the current parameter values
			auto reply = getCbor();
			reply.openMap();
			reply.addUtf8("shiftHz");
			reply.addFloat(shiftHz);
			reply.close();
			sendCborToWeb();
			return true;
		}
		if (!cbor.isMap()) return false;
		cbor.forEachPair([&](Cbor key, Cbor value){
			if (key.utf8View() == "shiftHz" && value.isNumber()) {
				shiftHz = value;
				_host.stateMarkDirty();
				// Ideally we'd emit a parameter-change event for this on the next flush, but this is easier for now
				_host.paramsRescan(CLAP_PARAM_RESCAN_VALUES);
			}
		});
		return !cbor.error();
	}
	
	static bool web_get_start(const clap_plugin_t* plugin, char* out_buffer, uint32_t out_buffer_capacity) {
		FreqShifter &self = *(FreqShifter *)plugin->plugin_data;
		std::strncpy(out_buffer, "web/ui.html", out_buffer_capacity);
		return true;
	}
	static bool web_receive(const clap_plugin_t* plugin, const void* buffer, uint32_t size) {
		FreqShifter &self = *(FreqShifter *)plugin->plugin_data;
		return self.webReceive(buffer, size);
	}
	clap_plugin_web extWeb{
		.get_start=web_get_start,
		.receive=web_receive
	};
	
	bool init() noexcept override {
		_host.getExtension(hostWeb, CLAP_EXT_WEB);
		return true;
	}
      const void *extension(const char *id) noexcept override {
		if (!strcmp(id, CLAP_EXT_WEB)) return &extWeb;
		return nullptr;
	}
private:
	signalsmith::hilbert::HilbertIIR<float> hilbert;
	float sampleRate = 0;
	float shiftPhase = 0;
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
