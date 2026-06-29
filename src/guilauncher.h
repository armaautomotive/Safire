// Copyright (c) 2026 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_GUI_LAUNCHER_H
#define SAFIRE_GUI_LAUNCHER_H

#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

inline bool safireExecutableFileExists(const std::string& path){
    return access(path.c_str(), X_OK) == 0;
}

inline bool safireGraphicalSessionAvailable(){
#ifdef __APPLE__
    return getenv("SSH_CONNECTION") == NULL && getenv("SSH_TTY") == NULL;
#elif defined(_WIN32)
    return true;
#else
    return getenv("DISPLAY") != NULL ||
        getenv("WAYLAND_DISPLAY") != NULL ||
        getenv("MIR_SOCKET") != NULL;
#endif
}

inline std::string safireShellQuote(const std::string& value){
    std::string quoted = "'";
    for(std::size_t i = 0; i < value.length(); i++){
        if(value[i] == '\''){
            quoted += "'\\''";
        } else {
            quoted += value[i];
        }
    }
    quoted += "'";
    return quoted;
}

inline std::string safireGuiLauncherPath(){
    std::vector<std::string> candidates;
    candidates.push_back("gui_client/run.sh");
    candidates.push_back("./gui_client/run.sh");
    candidates.push_back("../gui_client/run.sh");

    for(int i = 0; i < candidates.size(); i++){
        if(safireExecutableFileExists(candidates.at(i))){
            return candidates.at(i);
        }
    }
    return "";
}

inline bool safireLaunchGuiInterface(std::string& message){
    if(safireGraphicalSessionAvailable() == false){
        message = "No graphical window session detected. Start the GUI from a desktop session.";
        return false;
    }

    std::string launcher = safireGuiLauncherPath();
    if(launcher.length() == 0){
        message = "GUI launcher not found. Build the GUI with scripts/gui.sh first.";
        return false;
    }

    std::string command = safireShellQuote(launcher) + " >/tmp/safire-gui.log 2>&1 &";
    int result = system(command.c_str());
    if(result != 0){
        message = "GUI launch command failed. See /tmp/safire-gui.log for details.";
        return false;
    }

    message = "GUI launch requested.";
    return true;
}

#endif // SAFIRE_GUI_LAUNCHER_H
