#pragma once

#include "Common/CommonTypes.h"
#include <atomic>
#include <string>
#include <vector>
#include <thread>

class SlippiDirectCodes
{
    public:
        struct CodeInfo
        {
            std::string connectCode = "";
            std::string lastPlayed = "";
            bool isFavorite = false;
        };

        SlippiDirectCodes();
        ~SlippiDirectCodes();

        void ReadFile();
        void AddOrUpdateCode(std::string code);
        std::string get(u8 index);

    protected:
        void WriteFile();
        std::string getCodesFilePath();
        std::vector<CodeInfo> parseFile(std::string fileContents);
        std::vector<CodeInfo> directCodeInfos;
        
};