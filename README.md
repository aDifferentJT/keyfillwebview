# keyfillwebview

keyfillwebview renders any web page in key/fill for use with a hardware switcher.

Binary releases should be available in [Releases](https://github.com/nixCodeX/keyfillwebview/releases).
There are versions for Linux, macOS and Windows.

## Operation

When you open keyfillwebview it will display a page giving the URL to go to change the settings.

### System

This section contains some controls for the server itself

#### Shutdown

This shuts down the computer rendering the graphics.

Note that this shuts down the OS but does not power-off the computer, this is because some BIOSes support auto power-on after unexpected power loss so if you shut down but do not power-off until you remove power then it will automatically start up after power is restored.

### Loaded Page

Here you will find a text box for the URL to display and controls to change it

#### Load

This loads a new page if there is not currently a page loaded, that occurs if you are either still on the instructions page or the page you tried to load cannot be found.

#### Reload

This reloads the current page.

#### Reload Ignoring Cache

This reloads the current page ignoring the cache.

#### Force Load

This loads a new page regardless of current status.

#### Set Default

This sets a page to be loaded on startup instead of the instructions.

#### Reset

This goes back to displaying the instructions.

### Visibilty

This section allows you to control the visibility of the output, there are 2 modes

#### Show

This will show the output.

#### Clear

This will hide the output, making it transparent and showing anything underneath in the keyer.

### NDI

This section allows you to control the NDI layer

#### Source

This allows you to select the NDI source to use.

#### Show

This will show the NDI source.

#### Hide

This will hide the NDI source.

### Upload Video

This is experimental

### Play Video

This is experimental

## API

There are 6 methods in the API, 5 of them corresponding to the above [overations](#operation), all are performed by sending HTTP POST requests to the URL given on the instructions page, which will be on port 8080.

#### `/shutdown`

This is as the [shutdown](#shutdown) button.
It returns a 501 Not Implemented error if the server is running on an OS for which shutting down isn't yet supported.
It returns a 500 Internal Server Error if there is any other problem initiating the shutdown.

At the moment this is only implemented on Windows.

#### `/load`

This is as the [load](#load) button and the URL is taken from the body of the request.
It returns a 403 Forbidden error if there is a page currently being displayed.
It returns a 503 Service Unavailable error if the browser has not yet been loaded.

#### `/reload`

This is as the [reload](#reload) button.
It returns a 503 Service Unavailable error if the browser has not yet been loaded.

#### `/reload_ignoring_cache`

This is as the [reload_ignoring_cache](#reload-ignoring-cache) button.
It returns a 503 Service Unavailable error if the browser has not yet been loaded.

#### `/force_load`

This is as the [force_load](#force-load) button and the URL is taken from the body of the request.
It returns a 503 Service Unavailable error if the browser has not yet been loaded.

#### `/set_default`

This is as the [set_default](#set-default) button and the URL is taken from the body of the request.

#### `/is_active`

This queries whether there is a page currently being displayed and returns "true" if so and "false" if not.
It returns a 503 Service Unavailable error if the browser has not yet been loaded.

#### `/reset`

This is as the [reset](#reset) button.
It returns a 503 Service Unavailable error if the browser has not yet been loaded.

#### `/show`

This is as the [show](#show) button.

#### `/clear`

This is as the [clear](#clear) button.

#### `/show_ndi`

This is as the [NDI/Show](#show-1) button and the source name is taken from the body of the request.

#### `/hide_ndi`

This is as the [NDI/Hide](#hide) button.

## Building

This should build as any cmake project does, though on windows the CEF and SDL2 directories are hard coded so you will have to change those in CMakeLists.txt.
