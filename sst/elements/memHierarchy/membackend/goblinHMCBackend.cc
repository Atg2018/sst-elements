
#include <sst_config.h>
#include <membackend/goblinHMCBackend.h>

GOBLINHMCSimBackend::GOBLINHMCSimBackend(Component* comp, Params& params) : MemBackend(comp, params) {
	int verbose = params.find_integer("verbose", 0);

	output = new Output("HMCBackend: ", verbose, 0, Output::STDOUT);

	hmc_dev_count    = (uint32_t) params.find_integer("device_count", 1);
	hmc_link_count   = (uint32_t) params.find_integer("link_count", 4);
	hmc_vault_count  = (uint32_t) params.find_integer("vault_count", 1);
	hmc_queue_depth  = (uint32_t) params.find_integer("queue_depth", 32);
	hmc_bank_count   = (uint32_t) params.find_integer("bank_count", 1);
	hmc_dram_count   = (uint32_t) params.find_integer("dram_count", 1);
	hmc_capacity_per_device = (uint32_t) params.find_integer("capacity_per_device", 4);
	hmc_xbar_depth   = (uint32_t) params.find_integer("xbar_depth", 4);
	hmc_max_req_size = (uint32_t) params.find_integer("max_req_size", 64);
	hmc_trace_level  = (uint32_t) params.find_integer("trace", 0);
	hmc_tag_count    = (uint32_t) params.find_integer("tag_count", 64);

	hmc_trace_file   = params.find_string("trace_file", "hmc-trace.out");

	output->verbose(CALL_INFO, 1, 0, "Initializing HMC...\n");
	int rc = hmcsim_init(&the_hmc,
		hmc_dev_count,
		hmc_link_count,
		hmc_vault_count,
		hmc_queue_depth,
		hmc_bank_count,
		hmc_dram_count,
		hmc_capacity_per_device,
		hmc_xbar_depth);

	if(0 != rc) {
		output->fatal(CALL_INFO, -1, "Unable to initialize HMC back end model.\n");
	} else {
		output->verbose(CALL_INFO, 1, 0, "Initialized successfully.\n");
	}

	output->verbose(CALL_INFO, 1, 0, "Populating tag entries allowed at the controller, max tag count is: %" PRIu32 \n", hmc_tag_count;
	for(uint32_t i = 0; i < hmc_tag_count; hmc_tag_count++) {
		tag_queue.push(i);
	}

	output->verbose(CALL_INFO, 1, 0, "Setting the HMC trace file to %s\n", hmc_trace_level.c_str());
	hmc_trace_file_handle = fopen(hmc_trace_file.c_str(), "wt");

	if(NULL == hmc_trace_file_handle) {
		output->fatal(CALL_INFO, -1, "Unable to create and open the HMC trace file at %s\n", hmc_trace_file.c_str());
	}

	rc = hmcsim_trace_handle(&the_hmc, hmc_trace_file_handle);
	if(rc > 0) {
		output->fatal(CALL_INFO, -1, "Unable to set the HMC trace file to %s\n", hmc_trace_level.c_str());
	} else {
		output->verbose(CALL_INFO, 1, 0, "Succesfully set the HMC trace file path.\n");
	}

	output->verbose(CALL_INFO, 1, 0, "Setting the trace level of the HMC to %" PRIu32 "\n", hmc_trace_level);
	rc = hmcsim_trace_level(&the_hmc, hmc_trace_level);

	if(rc > 0) {
		output->fatal(CALL_INFO, -1, "Unable to set the HMC trace level to %" PRIu32 "\n", hmc_trace_level);
	} else {
		output->verbose(CALL_INFO, 1, 0, "Successfully set the HMC trace level.\n");
	}

	output->verbose(CALL_INFO, 1, 0, "Setting the maximum HMC request size to: %" PRIu32 "\n", hmc_max_req_size);
	rc = hmcsim_util_set_all_max_blocksize(&the_hmc, hmc_max_req_size);

	if(rc > 0) {
		output->fatal(CALL_INFO, -1, "Unable to set maximum HMC request size to %" PRIu32 "\n", hmc_max_req_size);
	} else {
		output->verbose(CALL_INFO, 1, 0, "Successfully set maximum request size.\n");
	}

	output->verbose(CALL_INFO, 1, 0, "Initializing the HMC trace log...\n");
	rc = hmcsim_trace_header(&the_hmc);

	if(rc > 0) {
		output->fatal(CALL_INFO, -1, "Unable to write trace header information into the HMC trace file.\n");
	} else {
		output->verbose(CALL_INFO, 1, 0, "Wrote the HMC trace file header successfully.\n");
	}

	zeroPacket(hmc_packet);

	// We are done with all the config/
	output->verbose(CALL_INFO, 1, 0, "Completed HMC Simulation Backend Initialization.\n");
}

bool GOBLINHMCSimBackend::issueRequest(MemController::DRAMReq* req) {
	// We have run out of tags
	if(tag_queue.size() == 0) {
		output->verbose(CALL_INFO, 4, 0, "Will not issue request this call, tag queue has no free entries.\n");
		return false;
	}

	// Zero out the packet ready for us to populate it with values below
	zeroPacket(hmc_packet);

	output->verbose(CALL_INFO, 4, 0, "Issue request for address: %" PRIu64 "\n", addr);

	const uint8_t		req_dev  = 0;
	const uint64_t		addr     = req->baseAddr_ + req->amtInProcess_;
	const uint16_t          req_tag  = tag_queue->front(); tag_queue->pop();
	hmc_rsqrt_t       	req_type;

	if(req->isWrite_) {
		// We are issuing a write
		req_type = WR64;
	} else {
		req_type = RD64;
	}

	const uint8_t           req_link = 0;

	uint64_t                req_header (uint64_t) 0;
	uint64_t                req_tail = (uint64_t) 0;

	int rc = hmcsim_build_memrequest(&the_hmc,
			req_dev,
			addr,
			req_tag,
			req_type,
			req_link,
			hmc_payload,
			req_header,
			req_tail);

	if(rc > 0) {
		output->fatal(CALL_INFO, -1, "Unable to build a request for address: %" PRIu64 "\n", addr);
	}

	packet[0] = req_header;
	packet[1] = req_tail;

	rc = hmcsim_send(&the_hmc, &packet[0]);

	if(HMC_STALL == rc) {
		output->verbose(CALL_INFO, 2, 0, "Issue revoked by HMC, reason: cube is stalled.\n");

		// Restore tag for use later, remember this request did not succeed
		tag_queue->push(req_tag);
	} else if(0 == rc) {
		output->verbose(CALL_INFO, 4, 0, "Issue of request for address %" PRIu64 " successfully accepted by HMC.\n", addr);
	} else {
		output->fatal(CALL_INFO, -1, "Error issue request for address %" PRIu64 " into HMC.\n", addr);
	}

	return true;
}

void GOBLINHMCSimBackend::setup() {

}

void GOBLINHMCSimBackend::finish() {

}

void GOBLINHMCSimBackend::clock() {
	output->verbose(CALL_INFO, 8, 0, "Clocking HMC...\n");
	int rc = hmcsim_clock(&the_hmc);

	if(rc > 0) {
		output->fatal(CALL_INFO, -1, "Error: clock call to the HMC failed.\n");
	}

	// Call to process any responses from the HMC
	processResponses();
}

void GOBLINHMCSimBackend::processResponses() {
	int rc = HMC_OK;

	for(int i = 0; i < the_hmc->num_links; ++i) {
		output->verbose(CALL_INFO, 4, 0, "Pooling responses on link %d...\n", i);

		rc = HMC_OK;

		while(rc != HMC_STALL) {
			rc = hmcsim_recv(&the_hmc, cube?, i, &hmc_packet[0]);

			if(HMC_OK == rc) {
				uint64_t	resp_head = 0;
				uint64_t	resp_tail = 0;
				hmc_response_t	resp_type;
				uint8_t		resp_length = 0;
				uint16_t	resp_tag = 0;
				uint8_t		resp_return_tag = 0;
				uint8_t		resp_src_link = 0;
				uint8_t		resp_rrp = 0;
				uint8_t		resp_frp = 0;
				uint8_t		resp_seq = 0;
				uint8_t		resp_dinv = 0;
				uint8_t		resp_errstat = 0;
				uint8_t		resp_rtc = 0;
				uint32_t	resp_crc = 0;

				int decode_rc = hmcsim_decode_memresponse(&the_hmc,
					&hmc_packet[0],
					&resp_head,
					&resp_tail,
					&hmc_response_t,
					&resp_length,
					&resp_tag,
					&resp_return_tag,
					&resp_src_link,
					&resp_rrp,
					&resp_frp,
					&resp_seq,
					&resp_dinv,
					&resp_errstart,
					&resp_rtc,
					&resp_crc);

				if(HMC_OK == decode_rc) {
					output->verbose(CALL_INFO, 4, 0, "Successfully decoded an HMC memory response for tag: %" PRIu16 "\n", resp_tag);

					std::map<uint16_t, MemController::DRAMReq*>::iterator locate_tag = tag_req_map->find(resp_tag);
					if(locate_tag == tag_req_map->end()) {
						output->fatal(CALL_INFO, -1, "Unable to find tag: %" PRIu16 " in the tag/request lookup table.", resp_tag);
					} else {
						MemController::DRAMReq* orig_req = locate_tag->second;

						output->verbose(CALL_INFO, 4, 0, "Matched tag %" PRIu16 " to request for address: %" PRIu64 "\n",
							resp_tag, (orig_req->baseAddr_ + orig_req->amtInProcess_));

						// Pass back to the controller to be handled, HMC sim is finished with it
						ctrl->handleMemResponse(orig_req);

						// Clear element from our map, it has been processed so no longer needed
						tag_req_map->erase(resp_tag);

						// Put the available tag back into the queue to be used
						tag_queue->push(resp_tag);
					}
				} else if(HMC_STALL == decode_rc) {
					// Should this situation happen? We have pulled the request now we want it decoded, if it stalls is it lost?
					output->verbose(CALL_INFO, 8, 0, "Attempted to decode an HMC memory request but HMC returns stall.\n");
				} else {
					output->fatal(CALL_INFO, -1, "Error: call to decode an HMC sim memory response returned an error code.\n");
				}
			} else if(HMC_STALL == rc) {
				output->verbose(CALL_INFO, 8, 0, "Call to HMC simulator recv returns a stall, no requests in flight.\n");
			} else {
				output->fatal(CALL_INFO, -1, "Error attempting to call a recv pool on the HMC simulator.\n);
			}
		}
	}
}

GOBLINHMCSimBackend::~GOBLINHMCSimBackend() {
	output->verbose(CALL_INFO, 1, 0, "Freeing the HMC resources...\n");
	hmc_free(&the_hmc);

	output->verbose(CALL_INFO, 1, 0, "Closing HMC trace file...\n");
	fclose(hmc_trace_file_handle);

	output->verbose(CALL_INFO, 1, 0, "Completed.\n");
	delete output;
}

void GOBLINHMCSimBackend::zeroPacket(uint64_t* packet) const {
	for(int i = 0; i < HMC_MAX_UQ_PACKET; ++i) {
		packet[i] = (uint64_t) 0;
	}
}
