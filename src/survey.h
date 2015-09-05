#ifndef BITCOIN_SURVEY_H
#define BITCOIN_SURVEY_H

#include <string>
#include <memory>

class CScheduler;
class CurlWrapper;

struct VersionNotifier {
    void Notify(const std::string& newver, bool isImportant);

    private:
        virtual void ShowUIWarning(const std::string&);
        virtual void ShowUIInfo(const std::string&);
        virtual void SetMiscWarning(const std::string&);
        virtual void Log(const std::string&);
};

class Survey {
    public:
        Survey(CScheduler&, std::auto_ptr<VersionNotifier>, std::auto_ptr<CurlWrapper>);
        ~Survey();

    private:
        std::auto_ptr<VersionNotifier> verNotifier;
        std::auto_ptr<CurlWrapper> curl;

        void pollSurvey();
};

#endif
