#include "user_config.h"
#include <SmingCore/SmingCore.h>
#include "AppSettings.h"
#include "ota_update.h"
#include "web_ipconfig.h"
#include "mavbridge.h"
#include "uptime.h"

HttpServer server;
FTPServer ftp;


void onIndex(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("status.html");
	auto &vars = tmpl->variables();

    vars["sw_ver"] = SW_VER;
    vars["hw_ver"] = HW_VER;

    vars["baud_rate"] = AppSettings.baud_rate;
    vars["mav_port_in"] = AppSettings.mav_port_in;
    vars["mav_port_out"] = AppSettings.mav_port_out;
    
    uint32_t net_rcvd, uart_rcvd;
    mavbridge_get_status(uart_rcvd, net_rcvd);
    
    vars["uart_pkts_rcvd"] = uart_rcvd;
    vars["net_pkts_rcvd"] = net_rcvd;

    if (WifiAccessPoint.isEnabled()) {
        vars["ap_ssid"] = AppSettings.ap_ssid;
        vars["ap_ip"] = WifiAccessPoint.getIP().toString();
    } else {
        vars["ap_ssid"] = "N/A";
        vars["ap_ip"] = "0.0.0.0";
    }
    if (AppSettings.ap_password.length() == 0)
        vars["ap_password"] = "(none)";
    else
        vars["ap_password"] = AppSettings.ap_password;

    vars["uptime"] = uptime_string();

    if (WifiStation.isEnabled()) {
        vars["client_ssid"] = WifiStation.getSSID();
        vars["client_ip"] = WifiStation.getIP().toString();
        vars["client_gw_ip"] = WifiStation.getNetworkGateway().toString();
        if (WifiStation.isConnected())
            vars["client_status"] = "Connected";
        else
            vars["client_status"] = "Not connected";

    } else {
        vars["client_ssid"] = "N/A";
        vars["client_ip"] = "0.0.0.0";
        vars["client_status"] = "Not connected";
    }


	response.sendTemplate(tmpl); // will be automatically deleted
}

void startWebServer()
{
	server.listen(80);
	server.addPath("/", onIndex);

	server.addPath("/networks", onIpConfig);
	server.addPath("/settings", onSettings);

	server.addPath("/ajax/get-networks", onAjaxNetworkList);
	server.addPath("/ajax/connect", onAjaxConnect);
	server.setDefaultHandler(onFile);
}

void startFTP()
{
	if (!fileExist("status.html"))
		fileSetContent("status.html", "<h3>Please connect to FTP and upload files from folder 'web/build' (details in code)</h3>");

	// Start FTP server
	ftp.listen(21);
	ftp.addUser("me", "123"); // FTP account
}

// Will be called when system initialization was completed
void startServers()
{
	startFTP();
	startWebServer();
}



void webserver_init()
{

    uptime_init();

    WifiStation.enable(true);

	if (AppSettings.exist() && AppSettings.ssid.length() != 0 && \
            AppSettings.password.length() >= 8)
	{
		WifiStation.config(AppSettings.ssid, AppSettings.password);
		if (!AppSettings.dhcp && !AppSettings.ip.isNull())
			WifiStation.setIP(AppSettings.ip, AppSettings.netmask, AppSettings.gateway);
	}


	WifiStation.startScan(networkScanCompleted);
    
    if (AppSettings.ap_ssid.length() == 0) {
        //autogenerated SSID
        AppSettings.ap_ssid =  "MAVBridge-" + WifiAccessPoint.getMAC();
        AppSettings.save();
    }

    // Start AP for configuration

    WifiAccessPoint.enable(true);
    AUTH_MODE mode = AUTH_OPEN;
    String password = "";

    if (AppSettings.ap_password.length() >= 8) {
        mode = AUTH_WPA2_PSK;
        password = AppSettings.ap_password;
    }

    WifiAccessPoint.config(AppSettings.ap_ssid, password, mode);

	// Run WEB server on system ready
	System.onReady(startServers);
}


