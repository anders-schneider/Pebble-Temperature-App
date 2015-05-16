/******  Listen for messages from watch app  ******/ Pebble.addEventListener("appmessage",
   function(e) {
     if (e.payload) {
       if (e.payload.current_temp) {
         // Get current temperature
         sendToServer(0);
       } else if (e.payload.statistics) {
         // Get temperature statistics
         sendToServer(1);
       } else if (e.payload.standby) {
         // Enter standy mode
         sendToServer(2);
       } else if (e.payload.wake_up) {
         // Exit standby mode
         sendToServer(3);
       } else if (e.payload.celsius) {
         // Change units to Celsius
         sendToServer(4);
       } else if (e.payload.fahrenheit) {
         // Change units to Fahrenheit
         sendToServer(5);
       } else if (e.payload.dance) {
         // Enter dance mode
         sendToServer(6);
       } else if (e.payload.stop_dancing) {
         // Exit dance mode
         sendToServer(7);
       } else {
         // Key not recognized
         Pebble.sendAppMessage({ "0": "nokey" });
       }
     } else {
       // No message content
       Pebble.sendAppMessage({ "0": "nopayload" });
     }
   }
);


/*******  Sends requests to server, parses response and sends to watch app  *******/ function sendToServer(key) {

   // Set up http request
	var req = new XMLHttpRequest();
	var ipAddress = "158.130.110.84"; // Hard coded IP address
	var port = "3001"; // Same port specified as argument to server
	var url = "http://" + ipAddress + ":" + port + "/" + key;
	var method = "GET";
	var async = true;

   // Handles response from server, passes response on to watch app
	req.onload = function(e) {
       var response = JSON.parse(req.responseText);
       if (response) {
           // Pass on response to watch app
           if (response.cur_temp) {
               // Current temperature
               Pebble.sendAppMessage({ "0": response.cur_temp });
           } else if (response.stats) {
               // Temperature statistics
               Pebble.sendAppMessage({ "1": response.stats });
           } else if (response.dance_on) {
               // Dance mode enabled
               Pebble.sendAppMessage({ "2": response.dance_on });
           } else if (response.not_connected_to_arduino) {
               // Unable to connect to arduino
               Pebble.sendAppMessage({ "3": 
response.not_connected_to_arduino });
           } else if (response.no_stats_data) {
               // No usable temperature statistics
               Pebble.sendAppMessage({ "404": response.no_stats_data});
           } else {
               // Key not recognized
               Pebble.sendAppMessage({ "999": "no message" });
           }
       }
	};

   // Handle error in http request (e.g. Unable to connect to server)
   // Just pass on error key (13) to the watch app
   req.onerror = function(e) {
       Pebble.sendAppMessage({"13" : "This is an error"});
   };

   // Send http request
   req.open(method, url, async);
   req.send(null);
}