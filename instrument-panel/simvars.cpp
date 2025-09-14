#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <allegro5/allegro.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#endif
#include "simvars.h"

const char* InstrumentPanelGroup = "Instrument Panel";
const char* InstrumentPanelHost = "Host";
const char* InstrumentPanelListenPort = "Listen Port";
const char *DataLinkGroup = "Data Link";
const char *DataLinkHost = "Host";
const char *DataLinkListenPort = "Listen Port";
const char* DataRateFps = "Data Rate FPS";
const char *MonitorGroup = "Monitor";
const char *MonitorStartOn = "StartOn";
const char* MonitorFullscreen = "Fullscreen";
const char* MonitorWidth = "Width";
const char* MonitorHeight = "Height";
const char* MonitorPositionX = "PositionX";
const char* MonitorPositionY = "PositionY";

extern const char* SimVarDefs[][2];
bool prevConnected = false;
int dataSize;
Request request;
char deltaData[8192];
int nextFull = 0;

void dataLink(simvars*);
void identifyAircraft(char* aircraft);
void receiveDelta(char* deltaData, int deltaSize, char* simVarsPtr);
void showError(const char* msg);
void fatalError(const char* msg);

simvars::simvars(const char *customSettings)
{
    if (customSettings == NULL) {
        strcpy(settingsFile, globals.SettingsFile);
    }
    else if (strchr(customSettings, '/') == NULL && strchr(customSettings, '\\') == NULL) {
        sprintf(settingsFile, "%s%s", globals.SettingsDir, customSettings);
    }
    else {
        strcpy(settingsFile, customSettings);
    }

    loadSettings();

    // Resolve data link host with precedence: environment > settings > default
    // override settings data link host with environment variable
    const char* envHost = getenv("DATA_LINK_HOST");
    if (envHost != NULL) {
        strcpy(globals.dataLinkHost, envHost);
    }
    // If no environment variable and globals.dataLinkHost is empty, use default
    // if still not set, use default
    else if (strlen(globals.dataLinkHost) == 0) {
        strcpy(globals.dataLinkHost, "127.0.0.1");
    }

    // Start data link thread
    dataLinkThread = new std::thread(dataLink, this);
}

simvars::~simvars()
{
    if (strlen(globals.error) == 0) {
        saveSettings();
    }

    if (dataLinkThread) {
        // Wait for thread to exit
        dataLinkThread->join();
    }
}

void simvars::loadSettings()
{
    // Load settings from JSON file
    static char buf[16384] = { '\0' };

    // Read file in a single chunk
    FILE* infile = fopen(settingsFile, "r");
    if (infile) {
        int bytes = (int)fread(buf, 1, 16384, infile);
        buf[bytes] = '\0';
        fclose(infile);
    }
    else {
        printf("Settings file %s not found\n", settingsFile);
    }

    groupCount = 0;
    int level = 0;
    bool readingName = false;
    bool readingValue = false;
    char group[256];
    char name[256];
    char value[256];

    group[0] = '\0';
    int pos = 0;
    char ch;
    while ((ch = buf[pos]) != '\0') {
        if (ch == '{' && !readingName && !readingValue) {
            level++;
        }
        else if (ch == '}' && !readingName && !readingValue) {
            group[0] = 0;
            level--;
        }
        else if (ch == '"' && !readingValue) {
            if (readingName) {
                if (level == 1) {
                    strcpy(group, name);
                    name[0] = '\0';
                }
                else if (strcmp(name, "Centre") == 0) {
                    // Ignore "Centre"
                    while (buf[pos++] != '\n');
                }
                readingName = false;
            }
            else {
                name[0] = '\0';
                readingName = true;
            }
        }
        else if (ch == ':' && !readingName && level == 2) {
            value[0] = '\0';
            readingValue = true;
        }
        else if ((ch == ',' || ch == '\n') && readingValue && level == 2) {
            if (group[0] != '\0' && name[0] != '\0' && value[0] != '\0') {
                if (_stricmp(group, DataLinkGroup) == 0) {
                    if (_stricmp(name, DataLinkHost) == 0) {
                        strcpy(globals.dataLinkHost, value);
                    }
                    else if (_stricmp(name, DataLinkListenPort) == 0) {
                        globals.dataLinkListenPort = settingValue(value);
                    }
                    else if (_stricmp(name, DataRateFps) == 0) {
                        globals.dataRateFps = settingValue(value);
                    }
                }
                else if (_stricmp(group, InstrumentPanelGroup) == 0) {
                    if (_stricmp(name, InstrumentPanelHost) == 0) {
                        // Only override if DATA_LINK_HOST env var is not set
                        if (getenv("DATA_LINK_HOST") == NULL) {
                            strcpy(globals.dataLinkHost, value);
                        }
                    }
                    else if (_stricmp(name, InstrumentPanelListenPort) == 0) {
                        globals.instrumentPanelListenPort = atoi(value);
                    }
                }
                else if (_stricmp(group, MonitorGroup) == 0) {
                    if (_stricmp(name, MonitorStartOn) == 0) {
                        globals.startOnMonitor = atoi(value);
                    }
                    else if (_stricmp(name, MonitorFullscreen) == 0) {
                        globals.monitorFullscreen = settingValue(value);
                    }
                    else if (_stricmp(name, MonitorWidth) == 0) {
                        globals.monitorWidth = atoi(value);
                    }
                    else if (_stricmp(name, MonitorHeight) == 0) {
                        globals.monitorHeight = atoi(value);
                    }
                    else if (_stricmp(name, MonitorPositionX) == 0) {
                        globals.monitorPositionX = atoi(value);
                    }
                    else if (_stricmp(name, MonitorPositionY) == 0) {
                        globals.monitorPositionY = atoi(value);
                    }
                }
                else if (groupCount == 0 || strcmp(groups[groupCount - 1].name, group) != 0) {
                    // New group
                    strcpy(groups[groupCount].name, group);
                    int idx = settingIndex(name);
                    if (idx == -1) {
                        sprintf(globals.error, "Settings file group %s contains unknown attribute %s", group, name);
                    }
                    else {
                        strcpy(groups[groupCount].settingName[idx], name);
                        groups[groupCount].settingVal[idx] = settingValue(value);
                        groups[groupCount].settingsCount = 1;
                        groupCount++;
                    }
                }
                else {
                    // Existing group
                    int idx = settingIndex(name);
                    if (idx == -1) {
                        sprintf(globals.error, "Settings file group %s contains unknown attribute %s", group, name);
                    }
                    else {
                        strcpy(groups[groupCount - 1].settingName[idx], name);
                        groups[groupCount - 1].settingVal[idx] = settingValue(value);
                        groups[groupCount - 1].settingsCount++;
                    }
                }
            }
            readingValue = false;
        }
        else if (readingName) {
            int len = (int)strlen(name);
            name[len] = ch;
            name[len + 1] = '\0';
        }
        else if (readingValue && ch != '"') {
            int len = (int)strlen(value);
            if (len != 0 || ch != ' ') {
                value[len] = ch;
                value[len + 1] = '\0';
            }
        }

        pos++;
    }
}

void simvars::saveSettings()
{
    // Save settings to JSON file
    FILE* outfile = fopen(settingsFile, "w");
    if (outfile)
    {
        fprintf(outfile, "{\n");
        fprintf(outfile, "  \"%s\": {\n", InstrumentPanelGroup);
        fprintf(outfile, "    \"%s\": \"%s\",\n", InstrumentPanelHost, globals.dataLinkHost);
        fprintf(outfile, "    \"%s\": %d\n", InstrumentPanelListenPort, globals.instrumentPanelListenPort);
        fprintf(outfile, "  },\n");

        fprintf(outfile, "  \"%s\": {\n", DataLinkGroup);
        fprintf(outfile, "    \"%s\": \"%s\",\n", DataLinkHost, globals.dataLinkHost);
        fprintf(outfile, "    \"%s\": %d,\n", DataLinkListenPort, globals.dataLinkListenPort);
        fprintf(outfile, "    \"%s\": %d\n", DataRateFps, globals.dataRateFps);
        fprintf(outfile, "  },\n");

        fprintf(outfile, "  \"%s\": {\n", MonitorGroup);
        fprintf(outfile, "    \"%s\": %d,\n", MonitorStartOn, globals.startOnMonitor);
        if (globals.monitorFullscreen) {
            fprintf(outfile, "    \"%s\": true,\n", MonitorFullscreen);
        }
        else {
            fprintf(outfile, "    \"%s\": false,\n", MonitorFullscreen);
        }
        fprintf(outfile, "    \"%s\": %d,\n", MonitorWidth, globals.monitorWidth);
        fprintf(outfile, "    \"%s\": %d,\n", MonitorHeight, globals.monitorHeight);
        fprintf(outfile, "    \"%s\": %d,\n", MonitorPositionX, globals.monitorPositionX);
        fprintf(outfile, "    \"%s\": %d\n", MonitorPositionY, globals.monitorPositionY);
        fprintf(outfile, "  },\n");

        int idx = 0;
        while (idx < groupCount)
        {
            if (idx > 0) {
                // End of previous group
                fprintf(outfile, ",\n");
            }

            // If group is enabled save new (possibly modified) settings otherwise save orig settings
            if (groups[idx].settingVal[3] == 1) {
                saveGroup(outfile, groups[idx].name);
            }
            else {
                fprintf(outfile, "  \"%s\": {\n", groups[idx].name);
                if (groups[idx].settingsCount == 4) {
                    fprintf(outfile, "    \"Enabled\": false,\n");
                    fprintf(outfile, "    \"%s\": %ld,\n", groups[idx].settingName[0], groups[idx].settingVal[0]);
                    fprintf(outfile, "    \"%s\": %ld,\n", groups[idx].settingName[1], groups[idx].settingVal[1]);
                    fprintf(outfile, "    \"%s\": %ld\n", groups[idx].settingName[2], groups[idx].settingVal[2]);
                }
                else {
                    fprintf(outfile, "    \"Enabled\": false\n");
                }
                fprintf(outfile, "  }");
            }

            idx++;
        }

        // End of previous group
        fprintf(outfile, "\n");
        fprintf(outfile, "}\n");
        fclose(outfile);
    }
}

int simvars::settingIndex(const char* attribName)
{
    if (_stricmp(attribName, "Position X") == 0) {
        return 0;
    }
    else if (_stricmp(attribName, "Position Y") == 0) {
        return 1;
    }
    else if (_stricmp(attribName, "Size") == 0) {
        return 2;
    }
    else if (_stricmp(attribName, "Enabled") == 0) {
        return 3;
    }
    else {
        return -1;
    }
}

int simvars::settingValue(const char* value)
{
    if (_stricmp(value, "true") == 0) {
        return 1;
    }
    else {
        return atol(value);
    }
}

/// <summary>
/// Show offset of centre of instrument from centre of monitor in mm.
/// Adjust the formula to suit your monitor size. This is only required
/// if you are going to place plywood over your monitor and nedd to know
/// where the cutouts should be positioned to match your instrument layout.
/// </summary>
void simvars::showCentre(FILE *outfile, const char *group, int x, int y, int size)
{
    float sizeY = size;

    if (_stricmp(group, "Annunciator") == 0) {
        sizeY = size / 4;
    }

    float mX = (x + (size / 2.0) - (1920.0 / 2.0)) / 3.654;
    float mY = (y + (sizeY / 2.0) - (1080.0 / 2.0)) / 3.654;

    fprintf(outfile, ",\n");
    fprintf(outfile, "    \"Centre\": \"cx%+.1fmm,cy%+.1fmm\"", mX, mY);
}

void simvars::saveGroup(FILE *outfile, const char* group)
{
    bool foundGroup = false;

    int idx = 0;
    while (idx <= varCount)
    {
        if (idx < varCount && strcmp(varGroup[idx], group) == 0) {
            if (!foundGroup) {
                // Start of group
                foundGroup = true;
                fprintf(outfile, "  \"%s\": {\n", group);
                fprintf(outfile, "    \"Enabled\": true");
            }

            // Only settings (negative nums) should be saved to the file
            if (varOffset[idx] < 0) {
                if (_stricmp(varName[idx], "Position X") == 0) {
                    //showCentre(outfile, group, varVal[idx], varVal[idx + 1], varVal[idx + 2]);
                }
                fprintf(outfile, ",\n");
                fprintf(outfile, "    \"%s\": %.0f", varName[idx], varVal[idx]);
            }
        }
        else if (foundGroup) {
            // End of group
            fprintf(outfile, "\n");
            fprintf(outfile, "  }");
            return;
        }

        idx++;
    }
}

int simvars::getVarIdx(int offset)
{
    int idx = 0;
    while (idx < varCount)
    {
        if (varOffset[idx] == offset)
        {
            return idx;
        }

        idx++;
    }

    return -1;
}

bool simvars::isCorrectType(int idx)
{
    // Settings (for arranging) have negative values and variables (for simulating) have positive values
    if (globals.arranging && varOffset[idx] < 0 || globals.simulating && varOffset[idx] > 0) {
        return true;
    }

    return false;
}

void simvars::getNextVar()
{
    int idx = getVarIdx(currentVar);
    if (idx == -1)
    {
        if (varCount == 0) {
            return;
        }

        idx = varCount - 1;
    }

    int startIdx = idx;
    while (true) {
        if (idx == varCount - 1) {
            idx = 0;
        }
        else {
            idx++;
        }

        if (isCorrectType(idx)) {
            currentVar = varOffset[idx];
            return;
        }

        if (idx == startIdx) {
            return;
        }
    }
}

void simvars::getPrevVar()
{
    int idx = getVarIdx(currentVar);
    if (idx == -1)
    {
        if (varCount == 0) {
            return;
        }

        idx = 0;
    }

    int startIdx = idx;
    while (true) {
        if (idx == 0) {
            idx = varCount - 1;
        }
        else {
            idx--;
        }

        if (isCorrectType(idx)) {
            currentVar = varOffset[idx];
            return;
        }

        if (idx == startIdx) {
            return;
        }
    }
}

void simvars::adjustVar(double amount)
{
    int idx = getVarIdx(currentVar);
    if (idx == -1)
    {
        return;
    }

    if (globals.dataLinked && varOffset[idx] >= 0) {
        showError("Cannot adjust variables when data linked");
        return;
    }

    if (varIsBool[idx]) {
        if (varVal[idx] == 0) {
            varVal[idx] = 1;
        }
        else {
            varVal[idx] = 0;
        }
    }
    else {
        varVal[idx] += amount * varScaling[idx];
    }

    if (varOffset[idx] >= 0) {
        // Update real SimVar variable
        double *pVar = (double *)&simVars + varOffset[idx];
        *pVar = varVal[idx];
    }
}

char *simvars::view()
{
    static char text[256] = "\0";

    if (currentVar == 0) {
        getNextVar();
    }

    int idx = getVarIdx(currentVar);
    if (idx == -1)
    {
        return text;
    }

    if (!isCorrectType(idx)) {
        getNextVar();
    }

    if (globals.dataLinked && !globals.arranging) {
        // Update with real value
        double *pVar = (double *)&simVars + varOffset[idx];
        varVal[idx] = *pVar;
    }

    sprintf(text, "%s %s: %.0f", varGroup[idx], varName[idx], varVal[idx]);
    return text;
}

void simvars::doKeypress(int keycode)
{
    switch (keycode)
    {
    case ALLEGRO_KEY_UP:
        getPrevVar();
        return;

    case ALLEGRO_KEY_DOWN:
        getNextVar();
        return;

    case ALLEGRO_KEY_LEFT:
        adjustVar(-1);
        return;

    case ALLEGRO_KEY_RIGHT:
        adjustVar(1);
        return;

    case ALLEGRO_KEY_PAD_4:
        // Numpad Left
        adjustVar(-10);
        return;

    case ALLEGRO_KEY_PAD_6:
        // Numpad Right
        adjustVar(10);
        return;
    }
}

void simvars::addVar(const char* group, const char* name, bool isBool, double scaling, double val)
{
    // Convert SimVar name to address offset (number of doubles)
    int offset = 1;
    for (int i = 0;; i++) {
        if (SimVarDefs[i][0] == NULL) {
            sprintf(globals.error, "Unknown SimVar name: %s - %s", group, name);
            return;
        }

        if (strcmp(SimVarDefs[i][0], name) == 0) {
            break;
        }

        if (strcmp(SimVarDefs[i][1], "string32") == 0) {
            offset += 4;
        }
        else {
            offset++;
        }
    }

    // Must not already be added
    int idx = getVarIdx(offset);
    if (idx != -1)
    {
        sprintf(globals.error, "Duplicate var %s must be added to common instead of %s and %s", name, varGroup[idx], group);
        return;
    }

    strcpy(varGroup[varCount], group);
    strcpy(varName[varCount], name);
    varOffset[varCount] = offset;
    varIsBool[varCount] = isBool;
    varScaling[varCount] = scaling;
    varVal[varCount] = val;

    varCount++;
}

void simvars::addSetting(const char* group, const char* name)
{
    // Get value from loaded settings
    int val = 0;
    int groupNum = 0;
    while (groupNum < groupCount)
    {
        if (strcmp(groups[groupNum].name, group) == 0) {
            // Find setting
            int num = 0;
            while (num < groups[groupNum].settingsCount)
            {
                if (strcmp(groups[groupNum].settingName[num], name) == 0) {
                    val = groups[groupNum].settingVal[num];
                    break;
                }

                num++;
            }
            break;
        }

        groupNum++;
    }

    strcpy(varGroup[varCount], group);
    strcpy(varName[varCount], name);
    varOffset[varCount] = settingOffset--;
    varIsBool[varCount] = false;
    varScaling[varCount] = 1;
    varVal[varCount] = val;

    varCount++;
}

/// <summary>
/// Returns true if the specified instrument is enabled
/// </summary>
bool simvars::isEnabled(const char* group)
{
    // Get value from loaded settings
    int val = -1;
    int groupNum = 0;
    while (groupNum < groupCount)
    {
        if (strcmp(groups[groupNum].name, group) == 0) {
            val = groups[groupNum].settingVal[3];
            break;
        }

        groupNum++;
    }

    if (val == -1) {
        // Add missing instrument to settings file
        strcpy(groups[groupCount].name, group);
        strcpy(groups[groupCount].settingName[0], "Enabled");
        groups[groupCount].settingVal[0] = settingValue("false");
        groups[groupCount].settingsCount = 1;
        groupCount++;
    }

    return (val == 1);
}

/// <summary>
/// Reads the settings, initially from the json file.
/// If the settings are not yet in the json file use the supplied defaults.
/// </summary>
int * simvars::readSettings(const char* group, int defaultX, int defaultY, int defaultSize)
{
    static int vals[3];

    int idx = 0;
    while (idx < varCount)
    {
        if (strcmp(varGroup[idx], group) == 0)
        {
            // If size is 0, replace with default values
            if (varVal[idx+2] == 0)
            {
                varVal[idx] = defaultX;
                varVal[idx+1] = defaultY;
                varVal[idx+2] = defaultSize;
            }

            vals[0] = varVal[idx];
            vals[1] = varVal[idx+1];
            vals[2] = varVal[idx+2];

            return vals;
        }

        idx++;
    }

    vals[0] = defaultX;
    vals[1] = defaultY;
    vals[2] = defaultSize;

    return vals;
}

/// <summary>
/// Write event to Flight Sim with optional data value
/// </summary>
void simvars::write(EVENT_ID eventId, double value)
{
    if (!globals.dataLinked) {
        return;
    }

    writeRequest.requestedSize = sizeof(WriteData);
    writeRequest.writeData.eventId = eventId;
    writeRequest.writeData.value = value;

    if (writeSockfd == INVALID_SOCKET) {
        if ((writeSockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
            fatalError("Failed to create UDP socket for writing");
        }

        int opt = 1;
#ifdef _WIN32
        if (setsockopt(writeSockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
            printf("DataLink: Write socket setsockopt failed with error: %d\n", WSAGetLastError());
        }
#else
        if (setsockopt(writeSockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            printf("DataLink: Write socket setsockopt failed with error: %d\n", errno);
        }
#endif

        writeAddr.sin_family = AF_INET;
        writeAddr.sin_port = htons(globals.dataLinkListenPort);
        
        const char* hostAddr = globals.dataLinkHost;
        
        inet_pton(AF_INET, hostAddr, &writeAddr.sin_addr);
    }

    int bytes = sendto(writeSockfd, (char*)&writeRequest, sizeof(writeRequest), 0, (SOCKADDR*)&writeAddr, sizeof(writeAddr));
    if (bytes <= 0) {
        sprintf(globals.error, "Failed to write event %d", eventId);
    }
}

void resetConnection()
{
    dataSize = sizeof(SimVars);
    // printf("DataLink: dataSize = %zu bytes\n", dataSize);
    // fflush(stdout);
    request.requestedSize = dataSize;

    // Want full data on first connect
    request.wantFullData = 1;
    nextFull = globals.dataRateFps * 2;

    globals.dataLinked = false;
    globals.connected = false;
    globals.aircraft = NO_AIRCRAFT;
    strcpy(globals.lastAircraft, "");
}

/// <summary>
/// New data received so perform various checks
/// </summary>
void processData(simvars* thisPtr)
{
    globals.dataLinked = true;
    globals.connected = (thisPtr->simVars.connected == 1);

    identifyAircraft(thisPtr->simVars.aircraft);
}

/// <summary>
/// A separate thread constantly collects the latest
/// SimVar values from instrument-data-link.
/// </summary>
void dataLink(simvars* thisPtr)
{
    char errMsg[256];
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    int actualSize;
    int bytes;
    int selFail = 0;

    printf("DataLink: Starting data link thread...\n");

#ifdef _WIN32
    WSADATA wsaData;
    printf("DataLink: Initializing Windows Sockets...\n");
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0) {
        sprintf(errMsg, "DataLink: Failed to initialise Windows Sockets: %d\n", err);
        fatalError(errMsg);
    }
    printf("DataLink: Windows Sockets initialized successfully\n");
#endif

    // Create receive socket
    printf("DataLink: Creating receive socket...\n");
    SOCKET recvSock;
    recvSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recvSock == INVALID_SOCKET) {
#ifdef _WIN32
        printf("DataLink: Receive socket failed with error: %d\n", WSAGetLastError());
#else
        printf("DataLink: Receive socket failed with error: %d\n", errno);
#endif
        fatalError("DataLink: Failed to create receive UDP socket");
    }

    // Create send socket
    SOCKET sendSock;
    sendSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sendSock == INVALID_SOCKET) {
#ifdef _WIN32
        printf("DataLink: Send socket failed with error: %d\n", WSAGetLastError());
#else
        printf("DataLink: Send socket failed with error: %d\n", errno);
#endif
        fatalError("DataLink: Failed to create send UDP socket");
    }

    // Configure receive socket
    sockaddr_in recvAddr;
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(globals.instrumentPanelListenPort);
    // Restrict to localhost if host is 127.0.0.1
    if (strcmp(globals.dataLinkHost, "127.0.0.1") == 0) {
        recvAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    else {
        recvAddr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(recvSock, (SOCKADDR*)&recvAddr, sizeof(recvAddr)) == SOCKET_ERROR) {
#ifdef _WIN32
        printf("DataLink: Bind failed with error: %d\n", WSAGetLastError());
#else
        printf("DataLink: Bind failed with error: %d\n", errno);
#endif
        fatalError("DataLink: Failed to bind UDP socket");
    }
    printf("DataLink: Bound to port %d for receiving\n", globals.instrumentPanelListenPort);

    int opt = 1;
#ifdef _WIN32
    if (setsockopt(recvSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        printf("DataLink: setsockopt failed with error: %d\n", WSAGetLastError());
    }
    if (setsockopt(sendSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        printf("DataLink: setsockopt failed with error: %d\n", WSAGetLastError());
    }
#else
    if (setsockopt(recvSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("DataLink: setsockopt failed with error: %d\n", errno);
    }
    if (setsockopt(sendSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("DataLink: setsockopt failed with error: %d\n", errno);
    }
#endif

    sockaddr_in sendAddr;
    sendAddr.sin_family = AF_INET;
    sendAddr.sin_port = htons(globals.dataLinkListenPort);
    
    const char* hostAddr = globals.dataLinkHost;
    
    printf("DataLink: Will send requests to %s:%d...\n", hostAddr, globals.dataLinkListenPort);
    
    if (inet_pton(AF_INET, hostAddr, &sendAddr.sin_addr) <= 0) {
        sprintf(errMsg, "DataLink: Invalid server address: %s\n", hostAddr);
        fatalError(errMsg);
    }

    resetConnection();
    printf("DataLink: Connection reset, starting main loop\n");

    while (!globals.quit) {
        // Poll instrument data link
        //if (nextFull > 0) {
        //    nextFull--;
        //    request.wantFullData = 0;
        //}
        //else {
        //    nextFull = globals.dataRateFps * 2;
        //    request.wantFullData = 1;
        //}
        request.wantFullData = 1;
        // printf("DataLink: Request - RequestedSize: %d, WantFullData: %d\n", request.requestedSize, request.wantFullData);
        // printf("DataLink: Request - reqSelfSize: %d\n", sizeof(request));        
        // printf("DataLink: Request - RequestedSize: %d, WantFullData: %d\n", sizeof(request.requestedSize), sizeof(request.wantFullData));

        // fflush(stdout);
        // printf("DataLink: Sending request\n");
        
        // print request hex values
        // for (int i = 0; i < sizeof(request); i++) {
        //     printf("%02x ", ((unsigned char*)&request)[i]);
        // }
        // printf("\n");

        bytes = sendto(sendSock, (char*)&request, sizeof(request), 0, (SOCKADDR*)&sendAddr, sizeof(sendAddr));

        if (bytes > 0) {
            // printf("DataLink: Request sent\n");
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(recvSock, &fds);

            int sel = select(FD_SETSIZE, &fds, 0, 0, &timeout);
            if (sel > 0) {
                // Receive latest data (delta will never be larger than full data size)
                bytes = recv(recvSock, deltaData, dataSize, 0);
                // printf("DataLink: Received %d bytes\n", bytes);
                if (bytes == 4) {
                    // Data size mismatch
                    memcpy(&actualSize, &thisPtr->simVars, 4);
                    sprintf(errMsg, "DataLink: Requested %ld bytes but server has %ld bytes\n", request.requestedSize, actualSize);
                    //fatalError(errMsg);
                }
                else if (bytes > 0) {
                    if (bytes == dataSize) {
                        // Full data received
                        memcpy((char*)&thisPtr->simVars, deltaData, dataSize);
                    }
                    else {
                        // Delta received
                        receiveDelta(deltaData, bytes, (char*)&thisPtr->simVars);
                    }

                    processData(thisPtr);
                }
                else {
                    printf("DataLink: Receive failed\n");
                    bytes = SOCKET_ERROR;
                }
            }
            else {
                // Link can blip so wait for multiple failures
                selFail++;
                // printf("DataLink: Select failed %d times\n", selFail);
                if (selFail > 100) {
                    printf("DataLink: Multiple select failures detected\n");
                    bytes = SOCKET_ERROR;
                }
            }
        }
        else {
            printf("DataLink: Send failed\n");
            bytes = SOCKET_ERROR;
        }

        if (bytes == SOCKET_ERROR && globals.dataLinked) {
            printf("DataLink: Connection lost, resetting...\n");
            resetConnection();
        }

        // Control update rate
#ifdef _WIN32
        Sleep(1000 / globals.dataRateFps);
#else
        usleep(1000000 / globals.dataRateFps);
#endif
    }

    printf("DataLink: Cleaning up...\n");
    closesocket(recvSock);
    closesocket(sendSock);

#ifdef _WIN32
    WSACleanup();
#endif
    
    printf("DataLink: Thread terminated\n");
}
