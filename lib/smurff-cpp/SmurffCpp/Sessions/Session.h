#pragma once

#include <iostream>
#include <memory>

#include "BaseSession.h"
#include <SmurffCpp/Configs/Config.h>
#include <SmurffCpp/Priors/IPriorFactory.h>

namespace smurff {

class SessionFactory;

class Session : public BaseSession, public std::enable_shared_from_this<Session>
{
   //only session factory should call setFromConfig
   friend class SessionFactory;

public:
   Config config;
   int iter = -1; //index of step iteration

protected:
   Session()
   {
      name = "Session";
   }

protected:
   void setFromConfig(const Config& cfg);

   // execution of the sampler
public:
   void run() override;

protected:
   void init() override;

public:
   void step() override;

public:
   std::ostream &info(std::ostream &, std::string indent) override;

private:
   //save iteration
   void save(int isample);
   void printStatus(double elapsedi);

private:
   void appendToRootFile(std::string tag, std::string item, bool truncate) const;

   std::string getOptionsFileName() const;

   std::string getSamplePrefix(int isample) const;

   std::string getRootFileName() const;

public:
   virtual std::shared_ptr<IPriorFactory> create_prior_factory() const;
};

}