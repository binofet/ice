#include "ice/render/device.h"
#include "ice/render/metal/device.h"
#import <QuartzCore/CAMetalLayer.h>
#include <iostream>

bool borrowed_layer_ownership_test() {
	@autoreleasepool {
		CAMetalLayer          *layer = [CAMetalLayer layer];
		ice::platform::surface endpoint;
		endpoint.api           = ice::platform::surface_api::metal;
		endpoint.native_handle = (__bridge void *)layer;
		endpoint.width         = 64;
		endpoint.height        = 40;
		auto first             = ice::render::device::create(ice::render::backend_kind::metal);
		auto second            = ice::render::device::create(ice::render::backend_kind::metal);
		auto native            = ice::render::metal::device::create();
		if (!first || !second || !native)
			return false;
		auto attached = (*first)->attach(endpoint);
		if (!attached || (*second)->attach(endpoint) ||
		    (*native)->attach_layer(endpoint.native_handle, 64, 40)) {
			std::cerr << "A borrowed Metal layer accepted overlapping renderer owners\n";
			return false;
		}
		// A device wrapper may retire first, but its live surface still owns the lease.
		first->reset();
		if ((*second)->attach(endpoint) || !(*attached)->wait_idle())
			return false;
		attached->reset();
		auto replacement = (*native)->attach_layer(endpoint.native_handle, 64, 40);
		if (!replacement || (*second)->attach(endpoint))
			return false;
		replacement->reset();
		return bool((*second)->attach(endpoint));
	}
}
