#include <iostream>
#include <iomanip>
#include <cyclus.h>
#include <sqlite_back.h>
#include "prettyprint.hpp"

//ValToStr converts any data type to a string for printing 
std::string ValToStr(boost::spirit::hold_any val, cyclus::DbTypes type) {
  std::stringstream ss;
  switch(type){
    case cyclus::BOOL:
      ss << val.cast<bool>();
      break;
    case cyclus::INT:
      ss << val.cast<int>();
      break;
    case cyclus::FLOAT:
      ss << val.cast<float>();
      break;
    case cyclus::DOUBLE:
      ss << val.cast<double>();
      break;
    case cyclus::STRING:
      ss << val.cast<std::string>();
      break;
    case cyclus::VL_STRING:
      ss << val.cast<std::string>();
      break;
    case cyclus::BLOB:
      ss << val.cast<cyclus::Blob>().str();
      break;
    case cyclus::UUID:
      ss << val.cast<boost::uuids::uuid>();
      break;
  }
  return ss.str();
}

//formatrow pretty prints the table
std::string formatrow(std::vector<std::string>) {
 // must format columns one of these days 
}

//StringToBool converts a valid string to a boolean
bool StringToBool(std::string str) {
 	boost::algorithm::to_lower(str);
 	std::string lowstr;
 	for (std::string::size_type i = 0; i < str.length(); ++i) {
   	lowstr = std::tolower(str[i]);
 	}
 	if (str=="true" || str=="t" || str=="1") {
   	return true;
 	} else if (str=="false" || str=="f" || str=="0") {
   	return false;
 	}
}

//StrToType looks up data type of string value by querying the 
//table (because we can't do anything else just yet)
boost::spirit::hold_any StrToType(std::string valstr, std::string field, std::string table, std::string fname) {

  //initiate query for table in a database
  cyclus::FullBackend* fback = new cyclus::SqliteBack(fname);
  cyclus::QueryResult result = fback->Query(table, NULL);

  //find type of column
  bool fieldmatch = false;
  std::vector<std::string> cols = result.fields;
  cyclus::DbTypes type;
  for (int i = 0; i < cols.size(); ++i) {
    std::string cycfield = cols[i];
    if (cycfield==field) { 
      type = result.types[i];
      break;
    }
  }

  //give value a type
  boost::spirit::hold_any val;
  switch(type){
    case cyclus::BOOL:
      val = StringToBool(valstr);
      break;
    case cyclus::INT:
      val = static_cast<int>(strtol(valstr.c_str(), NULL, 10));
      break;
    case cyclus::FLOAT:
      val = strtof(valstr.c_str(), NULL);
      break;
    case cyclus::DOUBLE:
      val = strtod(valstr.c_str(), NULL);
      break;
    case cyclus::STRING:
      val = valstr;
      break;
    case cyclus::VL_STRING:
      val = valstr;
      break;
    case cyclus::BLOB:
      std::cout << "Derp, Blob not supported for filtering\n";
      break;
    case cyclus::UUID:
      val = boost::lexical_cast<boost::uuids::uuid>(valstr);
      break;
  }
  return val;
}

//prints a condition
void PrintCond(std::string field, std::string op, std::string valstr){
  std::cout << "filter conditions: " << field << " " << op  << " " << valstr << "\n";
}

//ParseCond separates the conditions string for formatting 
cyclus::Cond ParseCond(std::string c, std::string table, std::string fname) {
  std::vector<std::string> ops = {"<", ">", "<=", ">=", "==", "!="};

  //finds the logical operator in the string
  std::string op;
  bool exists = false;
  for (int i = 0; !exists; ++i) {
    op = ops[i];
    exists = c.find(op) != std::string::npos; 
  }

  //finds the location of the logical operator
  size_t i = c.find(op);

  //gives substrings separated by the location of the operator
  std::string field = c.substr(0, i);
  char* cop = (char*)op.c_str(); 
  size_t j = strlen(cop);
  boost::spirit::hold_any value;
  std::string valstr;
  if (j == 2) {
    valstr = c.substr(i+2);
    value = StrToType(valstr, field, table, fname);
  } else {
    valstr = c.substr(i+1);
    value = StrToType(valstr, field, table, fname);
  }

  //populates cyclus-relevant condition
  cyclus::Cond cond = cyclus::Cond(field, op, value);
  PrintCond(field, op, valstr);
  return cond;
}

int main(int argc, char* argv[]) {
  using std::cout;
  if (argc < 3) {
    cout << "Derp, need at least 2 arguments\n";
  }

  //get and print arguments
  std::string fname = std::string(argv[1]);
  cout << "file name: " << fname << "\n";
  std::vector<cyclus::Cond> conds;
  //for (int i = 3; i < argc; ++i) {
//    conds.push_back(ParseCond(std::string(argv[i]), table, fname));
//  }

//get table from cyclus; print SimId and columns
  cyclus::FullBackend* fback = new cyclus::SqliteBack(fname);
  cyclus::Recorder rec;
  rec.RegisterBackend(fback);
  cyclus::Timer ti;
  cyclus::Context* ctx = new cyclus::Context(&ti, &rec);

  cyclus::QueryResult comps;
  if (conds.size() == 0) {
    comps = fback->Query("Compositions", NULL);
  } else {
    comps = fback->Query("Compositions", &conds);
  }
  cyclus::QueryResult resrc;
  if (conds.size() == 0) {
    resrc = fback->Query("Resources", NULL);
  } else {
    resrc = fback->Query("Resources", &conds);
  }
  cout << "\n" << "SimID: "; 

  
  for (int i = 0; i < comps.rows.size(); ++i) {
    for (int j = 0; j < resrc.rows.size(); ++j) {
      if (comps.rows[i][0] != resrc.rows[j][0])
        continue;
      if (comps.rows[i][1] != resrc.rows[j][7])
        continue;
      ctx->NewDatum("MassVector")
         ->AddVal("SimId", comps.rows[i][0])
         ->AddVal("QualId", comps.rows[i][1])
         ->AddVal("NucId", comps.rows[i][2])
         ->AddVal("Mass", comps.rows[i][3] * resrc.rows[j][5])
         ->Record();
    } 
  }

  delete fback;
  return 0;
}

