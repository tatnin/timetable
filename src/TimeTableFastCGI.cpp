#include "infix_ostream_iterator.h"

#include <fastcgi2/component.h>
#include <fastcgi2/component_factory.h>
#include <fastcgi2/handler.h>
#include <fastcgi2/request.h>

#include <iostream>
#include <sstream>

#include <stdio.h>
#include <cstring>
#include <algorithm> 
#include <iterator>
#include <vector>

#include <cstdlib>
#include "mongo/client/dbclient.h"
#include "mongo/bson/bson.h"

class TimeSheet : virtual public fastcgi::Component, virtual public fastcgi::Handler
{
    public:
        mongo::DBClientConnection c;

        TimeSheet(fastcgi::ComponentContext *context) :
                fastcgi::Component(context)
        { }

        virtual void onLoad()
        {
            try {
                c.connect("localhost");
            } catch( const mongo::DBException &e ) {
                std::cout<< "connection faild" << std::endl;
            } 
        }

        virtual void onUnload(){ }

        void updateInfo(mongo::BSONObj p, mongo::BSONObj newp, std::stringstream &inStream){
            try {
                c.update("table.testData", p, newp, true);
                if (c.getLastError().length() != 0){
                    inStream << "Error update\n";    
                }
            } catch( const mongo::DBException &e ) {
                std::cout<< "update faild\t" << c.getLastError() << std::endl;
                inStream << "DB Error: update faild\n";
            }
        }

        void findInfo(mongo::BSONObj p, std::string &find_day, std::stringstream &inStream){
            try {
                std::auto_ptr<mongo::DBClientCursor> cursor = c.query("table.testData", p);
                std::vector<std::string> v;
                if (find_day.length()>0){
                    std::string sel;
                    while (cursor->more()){
                        sel = cursor->next().getField(find_day).toString();
                        std::cout << sel << std::endl;
                        if (sel.length()>0)
                            v.push_back(sel);
                    }
                }else{
                    while (cursor->more()){
                        v.push_back(cursor->next().toString());
                    }
                }
                inStream << "[";
                std::copy(v.begin(),v.end(), infix_ostream_iterator<std::string>(inStream, ","));
                inStream << "]";
            } catch( const mongo::DBException &e ) {
                std::cout<< "query faild\t" << c.getLastError() << std::endl;
                inStream << "DB Error\n";
            }
        }

        mongo::BSONObj searchInfoBObj(std::string &find_course, std::string &find_group){
            mongo::BSONObjBuilder b;
            if (find_course.length() > 0){
                b.append("course", find_course);
                if (find_group.length() > 0){
                    b.append("group", find_group);
                }
            }
            return b.obj();
        }

        virtual void handleRequest(fastcgi::Request *request, fastcgi::HandlerContext *context)
        {
            std::stringstream inStream;

            std::string str_url = request->getURI();
            std::istringstream ss(str_url);
            std::string token;
            std::string find_param, find_course, find_group, find_lecturer, find_day;
            while(std::getline(ss, token, '/')) {
                if (token == "timetable"){
                    continue;
                }
                if (token == "course"){
                    if (std::getline(ss, token, '/')){
                        find_course = token;
                    }
                } else if (token == "group"){
                    if (std::getline(ss, token, '/')){
                        find_group = token;
                    }
                }
                else if (token == "day"){
                    if (std::getline(ss, token, '/')){
                        find_day = token;
                    }
                }
            }

            fastcgi::DataBuffer buf = request->requestBody();
            if (!buf.empty()){
                if (find_course.length() > 0 && find_group.length() > 0){
                    std::string s;
                    buf.toString(s);
                    updateInfo(searchInfoBObj(find_course, find_group), mongo::fromjson(s), inStream);
                }
            } else {  
                findInfo(searchInfoBObj(find_course, find_group), find_day, inStream);        
            }
            request->setContentType("application/json");//("text/plain");
            std::stringbuf buffer(inStream.str());
            request->setHeader("Simple-Header", "TimeTable");
            request->write(&buffer);
        }
};

class HelloFastCGI : virtual public fastcgi::Component, virtual public fastcgi::Handler
{
    public:
        HelloFastCGI(fastcgi::ComponentContext *context) :
                fastcgi::Component(context) {}
        virtual void onLoad(){}
        virtual void onUnload(){}
        virtual void handleRequest(fastcgi::Request *request, fastcgi::HandlerContext *context)
        {
            request->setContentType("text/plain");
            std::stringbuf buffer("Hello " + (request->hasArg("name") ? request->getArg("name") : "stranger"));
            request->setHeader("Simple-Header", "Reply from HelloFastCGI");
            request->write(&buffer);
        }
};

FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
FCGIDAEMON_ADD_DEFAULT_FACTORY("TimeSheetFactory", TimeSheet)
FCGIDAEMON_ADD_DEFAULT_FACTORY("HelloFastCGIFactory", HelloFastCGI)
FCGIDAEMON_REGISTER_FACTORIES_END()
