## Other driver based LED Matrix Panels ##

Limited support for other panels exists, but requires this to be passed as a configuration option when using the library.

These panels require a special reset sequence before they can be used, check your panel chipset if you have issues. Refer to the example.


```
 	mxconfig.driver = HUB75_I2S_CFG::FM6126A;     
	mxconfig.driver = HUB75_I2S_CFG::ICN2038S;
	mxconfig.driver = HUB75_I2S_CFG::FM6124;
	mxconfig.driver = HUB75_I2S_CFG::MBI5124;	
```
