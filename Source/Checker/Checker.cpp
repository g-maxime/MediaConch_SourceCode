/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Checker.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
//TODO: check xml entities on input
//TODO: mmt
//TODO: reference_file
//TODO: multiples policies

//---------------------------------------------------------------------------
// Helpers
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
std::string xml_encode(const std::string& data)
{
    std::stringstream ss;
    for (size_t pos=0; pos<data.size(); pos++)
    {
        switch (data[pos])
        {
            case '\'': ss << "&apos;"; break;
            case '"': ss << "&quot;"; break;
            case '&': ss << "&amp;"; break;
            case '<': ss << "&lt;"; break;
            case '>': ss << "&gt;"; break;
            case '\t': ss << "&#x9;";
            case '\n': ss << "&#xA;"; break;
            case '\r':
                ss << "&#xA;";
                if (pos+1<data.size() && data[pos+1]=='\n') // translate the #xD #xA sequence to a single #xA character
                    pos++;
            break;
            default: if ((unsigned char)data[pos]>=0x20) ss << data[pos]; // Ignore others control characters
        }
    }
    return ss.str();
}

//---------------------------------------------------------------------------
bool compare(const std::string& value, const std::string& reference, const std::string& operand)
{
    char* val_end=NULL;
    double val = strtod(value.c_str(), &val_end);
    char* ref_end=NULL;
    double ref = strtod(reference.c_str(), &ref_end);

    if (operand == "<")
    {
        if (!strlen(val_end) && !strlen(ref_end))
            return val < ref;
        else
            return strcmp(value.c_str(), reference.c_str()) < 0;
    }
    else if (operand == "<=")
    {
        if (!strlen(val_end) && !strlen(ref_end))
            return val <= ref;
        else
            return strcmp(value.c_str(), reference.c_str()) <= 0;
    }
    else if (operand == "=")
    {
        if (!strlen(val_end) && !strlen(ref_end))
            return val == ref;
        else
            return strcmp(value.c_str(), reference.c_str()) == 0;
    }
    else if (operand == ">=")
    {
        if (!strlen(val_end) && !strlen(ref_end))
            return val >= ref;
        else
            return strcmp(value.c_str(), reference.c_str()) >= 0;
    }
    else if (operand == ">")
    {
        if (!strlen(val_end) && !strlen(ref_end))
            return val > ref;
        else
            return strcmp(value.c_str(), reference.c_str()) > 0;
    }

    return false;
}

//---------------------------------------------------------------------------
std::string indent(size_t level)
{
    return std::string( level*2, ' ');
}

//---------------------------------------------------------------------------
int tfsxml_next_named(tfsxml_string* tfsxml_priv, tfsxml_string* result, const char* name)
{
    while (!tfsxml_next(tfsxml_priv, result))
    {
        if (!tfsxml_strcmp_charp(*result, name))
            return 0;
    }

    return -1;
}

//---------------------------------------------------------------------------
std::string tokenize(const std::string& scope, const std::string& list, const std::string& delimiter)
{
    std::stringstream ss;

    //TODO: mmt
    if (scope.empty() || scope=="mi")
    {
        std::istringstream iss(list);
        for (std::string token; std::getline(iss, token, '/');)
            ss << "/mi:" << token;
    }

    return ss.str();
}


//---------------------------------------------------------------------------
namespace MediaConch {

//---------------------------------------------------------------------------
// Policy
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
PolicyChecker::PolicyElement::PolicyElement() : resolved(false), pass_count(0), fail_count(0)
{
}

//---------------------------------------------------------------------------
PolicyChecker::PolicyElement::~PolicyElement()
{
    while (children.size())
    {
        delete children.back();
        children.pop_back();
    }
}

//---------------------------------------------------------------------------
void PolicyChecker::PolicyElement::resolve()
{
    for (size_t pos=0; pos<children.size(); pos++)
    {
        if (children[pos]->result())
             pass_count++;
         else
             fail_count++;
    }

    resolved=true;
}

//---------------------------------------------------------------------------
bool PolicyChecker::PolicyElement::result()
{
    if(!resolved)
        resolve();

    return (type=="or" && pass_count) || (type=="and" && !fail_count);
}

//---------------------------------------------------------------------------
std::string PolicyChecker::PolicyElement::to_string(size_t level, bool verbose)
{
    std::stringstream ss;

    ss << indent(level) << "<policy name=\"" << xml_encode(name) << "\" type=\"" << xml_encode(type) << "\" rules_run=\"" << children.size() << "\" pass_count=\""
                        << pass_count << "\" fail_count=\"" << fail_count << "\" outcome=\"" << (result() ? "pass" : "fail") << "\">" << std::endl;
    level++;
    if (!description.empty())
        ss << indent(level) << "<description>" << description << "</description>" << std::endl;
    for (size_t pos=0; pos<tags.size(); pos++)
        ss << indent(level) << "<tag>" << tags[pos] << "</tag>" << std::endl;
    for (size_t pos=0; pos<children.size(); pos++)
        ss << children[pos]->to_string(level, verbose);
    level--;
    ss << indent(level) << "</policy>" << std::endl;

    return ss.str();
}

//---------------------------------------------------------------------------
// Rule
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
PolicyChecker::RuleElement::RuleElement() : resolved(false), pass(false)
{
}

//---------------------------------------------------------------------------
PolicyChecker::RuleElement::~RuleElement()
{
}

//---------------------------------------------------------------------------
void PolicyChecker::RuleElement::reset()
{
    pass=false;
    resolved=false;
    values.clear();
}

//---------------------------------------------------------------------------
void PolicyChecker::RuleElement::resolve()
{
    if (operand=="exists")
    {
        if (occurrence=="*")
            pass=!values.empty();
        else
            pass=values.find(occurrence)!=values.end();
    }
    else if (operand=="must not exist")
    {
        if (occurrence=="*")
            pass=values.empty();
        else
            pass=values.find(occurrence)==values.end();
    }
    else if (operand=="starts with")
    {
        if (occurrence=="*")
            for (std::map<std::string, std::string>::iterator it=values.begin(); it!=values.end(); it++)
                pass
                |=(it->second.rfind(requested, 0)==0);
        else
             pass=values.find(occurrence)!=values.end() && values[occurrence].rfind(requested, 0)==0;
    }
    else if (operand=="must no starts with")
    {
        if (occurrence=="*")
            for (std::map<std::string, std::string>::iterator it=values.begin(); it!=values.end(); it++)
                pass|=(it->second.rfind(requested, 0)!=0);
        else
            pass=values.find(occurrence)!=values.end() && values[occurrence].rfind(requested, 0) != 0; //TODO: check xslt behavior
    }
    else if (operand=="<" || operand=="<=" || operand=="=" || operand==">=" || operand==">")
    {
        if (occurrence=="*")
            for (std::map<std::string, std::string>::iterator it=values.begin(); it!=values.end(); it++)
                 pass|=compare(it->second, requested, operand);
        else
            pass=values.find(occurrence)!=values.end() && compare(values[occurrence], requested, operand);
    }

    resolved=true;
}

//---------------------------------------------------------------------------
bool PolicyChecker::RuleElement::result()
{
    if(!resolved)
        resolve();

    return pass;
}

//---------------------------------------------------------------------------
std::string PolicyChecker::RuleElement::to_string(size_t level, bool verbose)
{
    std::stringstream ss;
    bool outcome = result();

    ss << indent(level) << "<rule name=\"" << xml_encode(name) << "\"";
    if (!scope.empty())
        ss << " scope=\"" << xml_encode(scope) << "\"";
    ss << " value=\"" << xml_encode(field) << "\" tracktype=\"" << xml_encode(tracktype) << "\" occurrence=\"" << xml_encode(occurrence)
       << "\" operator=\"" << xml_encode(operand) << "\" xpath=\"" << xml_encode(xpath) << "\"";
    if (!outcome || verbose)
        ss << " requested=\"" << xml_encode(requested) << "\" actual=\"" << (values.size() ? xml_encode(values.begin()->second) : std::string()) << "\"";
    ss << " outcome=\"" << (outcome ? "pass" : "fail") << "\"/>" << std::endl;

    return ss.str();
}

//---------------------------------------------------------------------------
// PolicyChecker
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
PolicyChecker::PolicyChecker()
{
}

//---------------------------------------------------------------------------
PolicyChecker::~PolicyChecker()
{
    while (policies.size())
    {
        delete policies.back();
        policies.pop_back();
    }
}

//---------------------------------------------------------------------------
bool PolicyChecker::full_parse()
{
    // check for presence of _StringX or TimeCode* element in policy)
    for (size_t pos=0; pos<rules.size(); pos++)
    {
        std::string& field=rules[pos]->field;
        size_t index=field.find_last_not_of("0123456789");
        if ((index!=std::string::npos && index>=6 && field.substr(index-6, 7)=="_String") || field.find("TimeCode")==0)
            return true;
    }

    return false;
}

//---------------------------------------------------------------------------
PolicyChecker::RuleElement* PolicyChecker::parse_rule(tfsxml_string& tfsxml_priv)
{
    RuleElement* rule=new RuleElement();

    tfsxml_string attribute_name;
    tfsxml_string attribute_value;
    while (!tfsxml_attr(&tfsxml_priv, &attribute_name, &attribute_value))
    {
        if (!tfsxml_strcmp_charp(attribute_name, "name"))
            rule->name=tfsxml_decode(attribute_value);
        else if (!tfsxml_strcmp_charp(attribute_name, "scope"))
            rule->scope=tfsxml_decode(attribute_value);
        else if (!tfsxml_strcmp_charp(attribute_name, "value"))
            rule->field=tfsxml_decode(attribute_value);
        else if (!tfsxml_strcmp_charp(attribute_name, "tracktype"))
            rule->tracktype=tfsxml_decode(attribute_value);
        else if (!tfsxml_strcmp_charp(attribute_name, "occurrence"))
            rule->occurrence=tfsxml_decode(attribute_value);
        else if (!tfsxml_strcmp_charp(attribute_name, "operator"))
            rule->operand=tfsxml_decode(attribute_value);
    }

    tfsxml_string value;
    if (!tfsxml_value(&tfsxml_priv, &value))
        rule->requested=std::string(value.buf, value.len);

    if (rule->scope.empty() || rule->scope=="mi")
    {
        std::stringstream ss;
        ss << "mi:MediaInfo/mi:track[@type='" << rule->tracktype << "'][" << rule->occurrence << "]" << tokenize(rule->scope, rule->field, "/");
        //if (rule->operand == "=" || rule->operand.rfind("<", 0) == 0 || rule->operand.rfind(">", 0) == 0 || rule->operand.rfind("&", 0) == 0 /* &lt; &lt;=...*/) // TODO: disable also for string fields
        //    ss << rule->operand << "'" << rule->requested << "'";
        rule->xpath=ss.str();
    }

    rules.push_back(rule);

    return rule;
}

//---------------------------------------------------------------------------
PolicyChecker::PolicyElement* PolicyChecker::parse_policy(tfsxml_string& tfsxml_priv)
{
    PolicyElement* policy=new PolicyElement();

    tfsxml_string attribute_name;
    tfsxml_string attribute_value;
    while (!tfsxml_attr(&tfsxml_priv, &attribute_name, &attribute_value))
    {
        if (!tfsxml_strcmp_charp(attribute_name, "name"))
            policy->name=tfsxml_decode(attribute_value);
        else if (!tfsxml_strcmp_charp(attribute_name, "type"))
            policy->type=tfsxml_decode(attribute_value);
        else if (!tfsxml_strcmp_charp(attribute_name, "version"))
            policy->version=tfsxml_decode(attribute_value);
    }

   tfsxml_string result;
    if (!tfsxml_enter(&tfsxml_priv))
    {
        while (!tfsxml_next(&tfsxml_priv, &result))
        {
            if (!tfsxml_strcmp_charp(result, "description"))
            {
                tfsxml_string result;
                if (!tfsxml_value(&tfsxml_priv, &result))
                    policy->description=std::string(result.buf, result.len);
            }
            else if (!tfsxml_strcmp_charp(result, "tag"))
            {
                tfsxml_string result;
                if (!tfsxml_value(&tfsxml_priv, &result))
                    policy->tags.push_back(std::string(result.buf, result.len));
            }
            else if (!tfsxml_strcmp_charp(result, "policy"))
                policy->children.push_back(parse_policy(tfsxml_priv));
            else if (!tfsxml_strcmp_charp(result, "rule"))
                policy->children.push_back(parse_rule(tfsxml_priv));
        }
    }

    return policy;
}

//---------------------------------------------------------------------------
void PolicyChecker::parse_mi_track(tfsxml_string& tfsxml_priv)
{
    tfsxml_string result;

    if (!tfsxml_enter(&tfsxml_priv))
    {
        std::string tracktype;
        std::string tracknumber;
        std::vector<RuleElement*> local;
        tfsxml_string tfsxml_priv_save = tfsxml_priv;

        // Search for StreamKind
        while (!tfsxml_next_named(&tfsxml_priv, &result, "StreamKind"))
        {
            if (!tfsxml_value(&tfsxml_priv, &result))
            {
                tracktype=std::string(result.buf, result.len);
                break;
            }
        }
        tfsxml_priv=tfsxml_priv_save;

        // search for StreamCount
        while (!tfsxml_next_named(&tfsxml_priv, &result, "StreamCount"))
        {
            if (!tfsxml_value(&tfsxml_priv, &result))
            {
                tracknumber=std::string(result.buf, result.len);
                break;
            }
        }
        tfsxml_priv=tfsxml_priv_save;

        // Search rules
        for (size_t pos=0; pos<rules.size(); pos++)
        {
            if (!rules[pos])
                continue;

            if((rules[pos]->scope.empty() || rules[pos]->scope == "mi") && rules[pos]->tracktype==tracktype && (rules[pos]->occurrence== "*" || rules[pos]->occurrence==tracknumber))
                local.push_back(rules[pos]);
        }

        if (local.empty())
        {
            while (!tfsxml_next(&tfsxml_priv, &result)); //TODO: fix parsing
            return;
        }

        // Populate values
        while (!tfsxml_next(&tfsxml_priv, &result))
        {
            std::string field=std::string(result.buf, result.len);
            if (field!="extra" && !tfsxml_value(&tfsxml_priv, &result))
            {
                for (size_t pos=0; pos<local.size(); pos++) //TODO: return if empty;
                {
                    if (local[pos] && local[pos]->field==field)
                    {
                        local[pos]->values[tracknumber]=std::string(result.buf, result.len);
                        local[pos]=NULL;
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------------------------
void PolicyChecker::add_policy(const std::string& policy)
{
    tfsxml_string tfsxml_priv;
    tfsxml_string result;

    tfsxml_init(&tfsxml_priv, (const void*)policy.c_str(), policy.size());
    while (!tfsxml_next(&tfsxml_priv, &result))
    {
        if (!tfsxml_strcmp_charp(result, "policy"))
        {
            PolicyElement* p=parse_policy(tfsxml_priv);
            if(p)
                policies.push_back(p);
            break;
        }
    }
}

//---------------------------------------------------------------------------
int PolicyChecker::analyze(const std::string& report, bool verbose, std::string& out)
{
    std::stringstream ss;
    size_t level=0;

    ss << indent(level++) << "<MediaConch xmlns=\"https://mediaarea.net/mediaconch\" xmlns:mmt=\"https://mediaarea.net/micromediatrace\" xmlns:mi=\"https://mediaarea.net/mediainfo\" version=\"0.3\">" << std::endl;

    tfsxml_string tfsxml_priv;
    tfsxml_string result;
    tfsxml_init(&tfsxml_priv, (const void*)report.c_str(), report.size());
    while (!tfsxml_next_named(&tfsxml_priv, &result, "MediaArea"))
    {
        if (!tfsxml_enter(&tfsxml_priv))
        {
            while (!tfsxml_next_named(&tfsxml_priv, &result, "media"))
            {
                std::string media;
                tfsxml_string attribute_name;
                tfsxml_string attribute_value;
                while (!tfsxml_attr(&tfsxml_priv, &attribute_name, &attribute_value))
                {
                    if (!tfsxml_strcmp_charp(attribute_name, "ref"))
                        media=std::string(attribute_value.buf, attribute_value.len);
                }

                ss << indent(level++) << "<media ref=\"" << media << "\">" << std::endl;
                if (!tfsxml_enter(&tfsxml_priv))
                {
                    while (!tfsxml_next(&tfsxml_priv, &result))
                    {
                        if (!tfsxml_strcmp_charp(result, "MediaInfo"))
                        {
                            if (!tfsxml_enter(&tfsxml_priv))
                            {
                                while (!tfsxml_next_named(&tfsxml_priv, &result, "track"))
                                    parse_mi_track(tfsxml_priv);
                            }
                        }
                        else if (!tfsxml_strcmp_charp(result, "MicroMediaTrace"))
                        {
                            //TODO:
                        }
                    }
                }
                for (size_t pos=0; pos<policies.size(); pos++)
                    ss << policies[pos]->to_string(level, verbose);
                ss << indent(--level) << "</media>" << std::endl;

                // reset states
                for (size_t pos=0; pos < rules.size(); pos++)
                    rules[pos]->reset();
            }
        }
    }

    ss << indent(--level) << "</MediaConch>" << std::endl;

    out = ss.str();
    return 0;
}

}

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
EMSCRIPTEN_BINDINGS(mediaconch_policychecker) {
    emscripten::class_<MediaConch::PolicyChecker>("PolicyChecker")
        .constructor()
        .function("full_parse", &MediaConch::PolicyChecker::full_parse)
        .function("add_policy", &MediaConch::PolicyChecker::add_policy)
        .function("analyze", emscripten::optional_override([](MediaConch::PolicyChecker& self, const std::string& report, bool verbose) {
            std::string out;
            self.MediaConch::PolicyChecker::analyze(report, verbose, out);

            return out;
        }))
    ;
}
#endif //__EMSCRIPTEN__
