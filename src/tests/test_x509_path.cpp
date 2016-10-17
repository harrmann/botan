/*
* (C) 2006,2011,2012,2014,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include "tests.h"

#if defined(BOTAN_HAS_X509_CERTIFICATES)
  #include <botan/x509path.h>
  #include <botan/internal/filesystem.h>
#endif

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>

namespace Botan_Tests {

namespace {

#if defined(BOTAN_HAS_X509_CERTIFICATES) && defined(BOTAN_TARGET_OS_HAS_FILESYSTEM)

std::map<std::string, std::string> read_results(const std::string& results_file)
   {
   std::ifstream in(results_file);
   if(!in.good())
      throw Test_Error("Failed reading " + results_file);

   std::map<std::string, std::string> m;
   std::string line;
   while(in.good())
      {
      std::getline(in, line);
      if(line == "")
         continue;
      if(line[0] == '#')
         continue;

      std::vector<std::string> parts = Botan::split_on(line, ':');

      if(parts.size() != 2)
         throw Test_Error("Invalid line " + line);

      m[parts[0]] = parts[1];
      }

   return m;
   }

class X509test_Path_Validation_Tests : public Test
   {
   public:
      std::vector<Test::Result> run() override
         {
         std::vector<Test::Result> results;

         // Test certs generated by https://github.com/yymax/x509test

         std::map<std::string, std::string> expected =
            read_results(Test::data_file("x509test/expected.txt"));

         const Botan::Path_Validation_Restrictions default_restrictions;

         Botan::X509_Certificate root(Test::data_file("x509test/root.pem"));
         Botan::Certificate_Store_In_Memory trusted;
         trusted.add_certificate(root);

         for(auto i = expected.begin(); i != expected.end(); ++i)
            {
            Test::Result result("X509test path validation");
            const std::string filename = i->first;
            const std::string expected_result = i->second;

            std::vector<Botan::X509_Certificate> certs =
               load_cert_file(Test::data_file("x509test/" + filename));

            if(certs.empty())
               throw Test_Error("Failed to read certs from " + filename);

            Botan::Path_Validation_Result path_result = Botan::x509_path_validate(
               certs, default_restrictions, trusted,
               "www.tls.test", Botan::Usage_Type::TLS_SERVER_AUTH);

            if(path_result.successful_validation() && path_result.trust_root() != root)
               path_result = Botan::Path_Validation_Result(Botan::Certificate_Status_Code::CANNOT_ESTABLISH_TRUST);

            result.test_eq("validation result", path_result.result_string(), expected_result);
            results.push_back(result);
            }

         return results;
         }

   private:

      std::vector<Botan::X509_Certificate> load_cert_file(const std::string& filename)
         {
         Botan::DataSource_Stream in(filename);

         std::vector<Botan::X509_Certificate> certs;
         while(!in.end_of_data())
            {
            try {
              certs.emplace_back(in);
            }
            catch(Botan::Decoding_Error&) {}
            }

         return certs;
         }

   };

BOTAN_REGISTER_TEST("x509_path_x509test", X509test_Path_Validation_Tests);

class NIST_Path_Validation_Tests : public Test
   {
   public:
      std::vector<Test::Result> run() override;
   };

std::vector<Test::Result> NIST_Path_Validation_Tests::run()
   {
   std::vector<Test::Result> results;

   /**
   * Code to run the X.509v3 processing tests described in "Conformance
   *  Testing of Relying Party Client Certificate Path Proccessing Logic",
   *  which is available on NIST's web site.
   *
   * Known Failures/Problems:
   *  - Policy extensions are not implemented, so we skip tests #34-#53.
   *  - Tests #75 and #76 are skipped as they make use of relatively
   *    obscure CRL extensions which are not supported.
   */
   const std::string nist_test_dir = Test::data_dir() + "/nist_x509";

   try
      {
      // Do nothing, just test filesystem access
      Botan::get_files_recursive(nist_test_dir);
      }
   catch(Botan::No_Filesystem_Access&)
      {
      Test::Result result("NIST path validation");
      result.test_note("Skipping due to missing filesystem access");
      results.push_back(result);
      return results;
      }

   std::map<std::string, std::string> expected =
      read_results(Test::data_file("nist_x509/expected.txt"));

   for(auto i = expected.begin(); i != expected.end(); ++i)
      {
      const std::string test_name = i->first;
      const std::string expected_result = i->second;

      const std::string test_dir = nist_test_dir + "/" + test_name;

      Test::Result result("NIST path validation");

      const std::vector<std::string> all_files = Botan::get_files_recursive(test_dir);

      if(all_files.empty())
         {
         result.test_failure("No test files found in " + test_dir);
         results.push_back(result);
         continue;
         }

      Botan::Certificate_Store_In_Memory store;

      for(auto&& file : all_files)
         {
         if(file.find(".crt") != std::string::npos && file != "end.crt")
            {
            store.add_certificate(Botan::X509_Certificate(file));
            }
         else if(file.find(".crl") != std::string::npos)
            {
            Botan::DataSource_Stream in(file, true);
            Botan::X509_CRL crl(in);
            store.add_crl(crl);
            }
         }

      Botan::X509_Certificate end_user(test_dir + "/end.crt");

      Botan::Path_Validation_Restrictions restrictions(true);

      Botan::Path_Validation_Result validation_result =
         Botan::x509_path_validate(end_user,
                                   restrictions,
                                   store);

      result.test_eq(test_name + " path validation result",
                     validation_result.result_string(),
                     expected_result);

      results.push_back(result);
      }

   return results;
   }

BOTAN_REGISTER_TEST("x509_path_nist", NIST_Path_Validation_Tests);

#endif

}

}