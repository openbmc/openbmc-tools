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

#pragma once

#include <map>
#include <string>
#include <vector>

class XMLNode
{
  public:
    std::string tag;
    std::map<std::string, std::string> fields;
    std::vector<XMLNode*> children;
    std::vector<XMLNode*> interfaces;
    XMLNode(const std::string& t) : tag(t)
    {}

    void AddChild(XMLNode* x)
    {
        children.push_back(x);
    }

    void do_Print(int indent)
    {
        for (int i = 0; i < indent; i++)
            printf("  ");
        printf("%s", tag.c_str());
        if (fields["name"] != "")
        {
            printf(" name=[%s]", fields["name"].c_str());
        }
        printf("\n");
        for (XMLNode* ch : children)
        {
            ch->do_Print(indent + 1);
        }
    }

    void Print()
    {
        do_Print(0);
    }

    void SetName(const std::string& n)
    {
        fields["name"] = n;
    }
    
    std::vector<std::string> GetChildNodeNames();
    std::vector<std::string> GetInterfaceNames();
};

XMLNode* ParseXML(const std::string& sv);
void DeleteTree(XMLNode* x);
