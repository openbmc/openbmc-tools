// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "xmlparse.hpp"
#include "main.hpp"

int Munch(const std::string& sv, int* idx, std::string* out)
{
    if (*idx >= static_cast<int>(sv.size()))
        return -INVALID;
    while (::isspace(sv[*idx]))
    {
        (*idx)++;
    }
    int ret = 0;
    *out = "";
    int quote_state = 0; // 0: not seen, 1: seen opening quotation, 2: ended
    while (*idx < static_cast<int>(sv.size()))
    {
        const char ch = sv[*idx];
        if (::isspace(ch) && quote_state != 1)
        {
            break;
        }
        (*idx)++;
        if (ch == '<')
        {
            if (*idx < static_cast<int>(sv.size()) && sv[*idx] == '!')
            {
                ret = 10; // Comment
            }
            else if (*idx < static_cast<int>(sv.size()) && sv[*idx] == '/')
            {
                ret = 22; // Closing tag
                (*idx)++; // Skip the '/'
            }
            else
            {
                ret = 1; // <
            }
        }
        else if (ch == '>')
        {
            if (ret == 1)
            {
                ret = 12;
            } // < >
            else if (ret == 22)
            {}
            else
                ret = 2; //   >
            if (out->size() == 0)
            {
                (*idx)++;
            }
            break; // Do not consume
        }
        else if (ch == '\"')
        {
            ret = 3; //
            switch (quote_state)
            {
                case 0:
                {
                    quote_state = 1;
                    continue;
                }
                case 1:
                {
                    quote_state = 2;
                    break;
                }
            }
        }
        else if (ch == '/' && *idx < static_cast<int>(sv.size()) &&
                 sv[*idx] == '>')
        {
            ret = 22; // Closing tag
            (*idx)++;
            break;
        }
        else
        {
            out->push_back(ch);
        }
    }
    return ret;
}

XMLNode* ParseXML(const std::string& sv)
{
    int verbose = 0;
    char* v = getenv("VERBOSE");
    if (v)
    {
        verbose = std::atoi(v);
    }
    int idx = 0;
    std::string out;
    int res;
    std::vector<std::string> tags;
    std::vector<XMLNode*> nodestack;
    XMLNode* root = nullptr;
    if (verbose > 0)
    {
        printf("%s\n", sv.c_str());
    }
    while ((res = Munch(sv, &idx, &out)) != -INVALID)
    {
        if (res == 1 || res == 12)
        {
            XMLNode* newnode = new XMLNode(out);
            if (tags.empty())
            {
                root = newnode;
            }
            else
            {
                nodestack.back()->AddChild(newnode);
            }
            tags.push_back(out);
            nodestack.push_back(newnode);
        }

        // Add name (has to be before pop_back)
        if (out.find("name=") == 0)
        {
            nodestack.back()->SetName(out.substr(5));
        }

        if (res == 22 && tags.size() > 0)
        {
            tags.pop_back();
            nodestack.pop_back();
        }
        if (verbose >= 2)
        {
            printf("Munch %d %s, tags:", res, out.c_str());
            for (const std::string& x : tags)
            {
                printf(" %s", x.c_str());
            }
            printf("\n");
        }
    }
    return root;
}

void DeleteTree(XMLNode* x)
{
    for (XMLNode* ch : x->children)
    {
        DeleteTree(ch);
    }
    delete x;
}

std::vector<std::string> XMLNode::GetChildNodeNames()
{
    std::vector<std::string> ret;
    for (XMLNode* n : children)
    {
        if (n->tag == "node")
        {
            ret.push_back(n->fields["name"]);
        }
    }
    return ret;
}

std::vector<std::string> XMLNode::GetInterfaceNames()
{
    std::vector<std::string> ret;
    for (XMLNode* n : children)
    {
        if (n->tag == "interface")
        {
            ret.push_back(n->fields["name"]);
        }
    }
    return ret;
}