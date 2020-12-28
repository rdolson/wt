/*
 * Copyright (C) 2017 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#include "Wt/Dbo/backend/SOCI.h"

#include "Wt/Dbo/Exception.h"
#include "Wt/Dbo/Logger.h"
#include "Wt/Dbo/StringStream.h"

#include "Wt/Date/date.h"

#include <soci/soci.h>


#include <cstring>
#include <iostream>
#include <vector>

namespace Wt {
    namespace Dbo {

	LOGGER("Dbo.backend.SOCI");

	namespace backend {

	    class SOCIException : public Exception
	    {
	    public:
		SOCIException(const std::string& msg,
			      const std::string &sqlState = std::string())
		    : Exception(msg, sqlState)
		{ }
	    };

	    struct SOCI::Impl {
		Impl(const std::string &str)
		    : connectionString(str) {
		    resultBuffer.buf = (char*)malloc(256);
		    resultBuffer.size = 256;
		    std::cerr << "New impl\n";
		}
		
		Impl(const Impl &other)
		    : connectionString(other.connectionString) {
		    resultBuffer.buf = (char*)malloc(256);
		    resultBuffer.size = 256;
		    std::cerr << "New impl copy\n";
		}
		
		~Impl() {
		    free(resultBuffer.buf);
		}
		
		void connect() {
		    session.open(connectionString);
		    session.set_log_stream(&std::cerr);
		}

		soci::session session;
		std::string connectionString;
		struct ResultBuffer {
		    char *buf;
		    std::size_t size;
		} resultBuffer;
	    };

	    class SOCIStatement : public SqlStatement {
	    public:
	    
		enum type_key
		{
		    t_short,
		    t_int,
		    t_longlong,
		    t_double,
		    t_stdstring,
		    t_null,
		};
		union bind_val
		{
		    short v_short;
		    int v_int;
		    long long v_longlong;
		    double v_double;
		    const std::string *v_stdstring;
		};
		struct bind_data
		{
		    type_key key;
		    bind_val val;
		};
	      
		SOCIStatement(SOCI &conn, const std::string &sql)
		    : stmt_(conn.impl_->session),
		      sql_(sql),
		      impl_(conn.impl_),
		      lastId_(-1) {

		    std::cerr << "STMT: " << sql << "\n";
		}

		virtual ~SOCIStatement()
		{
		    stmt_.clean_up();
		    clear_bindings();
		}
		
		void clear_bindings()
		{
		    for (auto b: bindings_)
		    {
			if (b.key == t_stdstring && b.val.v_stdstring != nullptr)
			{
			    delete b.val.v_stdstring;
			    b.val.v_stdstring = nullptr;
			}
		    }
		    bindings_.clear();
		}
		
		virtual void reset() override
		{
		}
		
		virtual void bind(int column, const std::string &value) override
		{
		    std::cerr << "BIND STR " << column << " " << value << "\n";
		    bindings_.push_back({ t_stdstring, { .v_stdstring = new std::string { value } } });
		    // bindings_.push_back({ t_stdstring, { .v_stdstring = &value }} );
		}
		
		virtual void bind(int column, short value) override
		{
		    std::cerr << "BIND SHORT " << column << " " << value << "\n";
		    stmt_.exchange(soci::use(value));
		}

		virtual void bind(int column, int value) override
		{
		    std::cerr << "BIND INT " << column << " " << value << "\n";
		    bindings_.push_back({ t_int, { .v_int = value } });
		}

		virtual void bind(int column, long long value) override
		{
		    std::cerr << "BIND LL " << column << " " << value << "\n";
		    bindings_.push_back({ t_longlong, { .v_longlong = value } });
		}
		
		virtual void bind(int column, float value) override
		{
		    std::cerr << "BIND FLOAT " << column << " " << value << "\n";
		    bindings_.push_back({ t_double, { .v_double = static_cast<double>(value) } });
		}

		virtual void bind(int column, double value) override
		{
		    std::cerr << "BIND DOUBLE " << column << " " << value << "\n";
		    bindings_.push_back({ t_double, { .v_double = value } });
		}

		virtual void bind(
				  int column,
				  const std::chrono::system_clock::time_point& value,
				  SqlDateTimeType type) override
		{
		    std::cerr << "BIND TIME " << column << "\n";
		}

		virtual void bind(
				  int column,
				  const std::chrono::duration<int, std::milli>& value) override
		{
		    long long msec = value.count();
		    bind(column, msec);
		}
		
		virtual void bind(
				  int column,
				  const std::vector<unsigned char>& value) override
		{
		    std::cerr << "BIND VEC CHAR " << "\n";
		}
		
		virtual void bindNull(int column) override
		{
		    std::cerr << "BIND NULL" << "\n";
		    bindings_.push_back( { t_null } );
		}

		virtual void execute() override
		{
		    std::cerr<< "EXECUTE\n";
		    stmt_.alloc();
		    stmt_.prepare(sql_);

		    for (int i = 0; i < bindings_.size(); i++)
		    {
			auto &b = bindings_[i];
			switch (b.key)
			{
			case t_null:
			    std::cerr << "bind null "  << "\n";
			    {
				soci::indicator ind = soci::i_null;
				stmt_.exchange(soci::use(0, ind));
			    }
			    break;
			    
			case t_int:
			    std::cerr << "bind int " << b.val.v_int << "\n";
			    stmt_.exchange(soci::use(bindings_[i].val.v_int));
			    break;
			    
			case t_longlong:
			    std::cerr << "bind longlong " << b.val.v_longlong << "\n";
			    stmt_.exchange(soci::use(bindings_[i].val.v_longlong));
			    break;
			    
			case t_double:
			    std::cerr << "bind double " << b.val.v_double << "\n";
			    stmt_.exchange(soci::use(bindings_[i].val.v_double));
			    break;

			case t_stdstring:
			    std::cerr << "bind str " << *b.val.v_stdstring << "\n";
			    stmt_.exchange(soci::use(*b.val.v_stdstring));
			    break;
			    
			}
		    }

		    stmt_.define_and_bind();
		    stmt_.exchange_for_rowset(soci::into(row_));
		    stmt_.execute(false);

		    first_row_ = true;
		    iter_ = std::move(soci::rowset_iterator<soci::row>(stmt_, row_));

		    // for (; iter_ != iter_end_; ++iter_) {
		    // 	soci::row const& row = *iter_;
		    // 	int iv = row.get<int>(0);
		    // 	std::cerr << "got row ent " << iv << "\n";
		    // } 

		    long long id = 0;
		    bool x = impl_->session.get_last_insert_id("", id);
		    std::cerr << "got id " << id << " res=" << x << "\n";
		    lastId_ = id;

		    clear_bindings();
		}

		virtual long long insertedId() override
		{
		    std::cerr << "GET insertedId " << lastId_ << "\n";
		    return lastId_;
		}
		
		virtual int affectedRowCount() override
		{
		    std::cerr << "GET affectedRowCount\n";
		    return static_cast<int>(affectedRows_);
		}

		virtual bool nextRow() override
		{
		    if (first_row_)
		    {
			first_row_ = false;
		    }
		    else
		    {
			++iter_;
		    }
		    bool gotData = (iter_ != iter_end_);
					
		    std::cerr << "nextRow() " << gotData << "\n";
		    return gotData;
		}

		virtual int columnCount() const override
		{
		    std::cerr << "columnCount()\n";
		    return 0;
		}
		
		virtual bool getResult(int column, std::string *value, int /*size*/) override
		{
		    std::cerr << "getResult str\n";
		    *value = iter_->get<std::string>(column);
		    return true;
		}

		virtual bool getResult(int column, short * value) override
		{
		    std::cerr << "getResult short\n";
		    *value = iter_->get<short>(column);
		    return true;
		}

		virtual bool getResult(int column, int * value) override
		{
		    std::cerr << "getResult int\n";
		    *value = iter_->get<int>(column);
		    return true;
		}

		virtual bool getResult(int column, long long * value) override
		{
		    *value = iter_->get<int>(column);
		    std::cerr << "getResult long long\n";
		    return true;

		}

		virtual bool getResult(int column, float * value) override
		{
		    *value = iter_->get<float>(column);
		    std::cerr << "getResult float\n";
		    return false;
		}

		virtual bool getResult(int column, double * value) override
		{
		    *value = iter_->get<double>(column);
		    std::cerr << "getResult double\n";
		    return false;
		}

		virtual bool getResult(
				       int column,
				       std::chrono::system_clock::time_point *value,
				       SqlDateTimeType type) override
		{
		    return false;
		}

		virtual bool getResult(
				       int column,
				       std::chrono::duration<int, std::milli> *value) override
		{
		    long long msec;
		    bool res = getResult(column, &msec);
		    if (!res)
			return res;
		    
		    *value = std::chrono::duration<int, std::milli>(msec);
		    return true;
		}

		virtual bool getResult(
				       int column,
				       std::vector<unsigned char> *value,
				       int size) override
		{
		    return false;
		}

		virtual std::string sql() const override
		{
		    return sql_;
		}
		
	    private:

		soci::statement stmt_;
		soci::row row_;
		bool first_row_;
		soci::rowset_iterator<soci::row> iter_;
		soci::rowset_iterator<soci::row> iter_end_;
		std::string sql_;
		long long lastId_;
		SOCI::Impl *impl_;

		long long affectedRows_;

		std::vector<bind_data> bindings_;
		
		void checkColumnIndex(int column)
		{
		}
		
	    };

	    SOCI::SOCI()
		: impl_(0)
	    { }

	    SOCI::SOCI(const std::string &connectionString)
		: impl_(0)
	    {
		connect(connectionString);
	    }

	    SOCI::SOCI(const SOCI &other)
		: SqlConnection(other),
		  impl_(other.impl_ ? new Impl(*other.impl_) : 0)
	    {
		if (impl_)
		    impl_->connect();
	    }

	    SOCI::~SOCI()
	    {
		clearStatementCache();
		delete impl_;
	    }

	    std::unique_ptr<SqlConnection> SOCI::clone() const
	    {
		return std::unique_ptr<SqlConnection>(new SOCI(*this));
	    }

	    bool SOCI::connect(const std::string &connectionString)
	    {
		if (impl_)
		    throw SOCIException("Can't connect: already connected.");

		Impl *impl = new Impl(connectionString);
		try {
		    impl->connect();
		} catch (...) {
		    delete impl;
		    throw;
		}

		impl_ = impl;
		return true;
	    }

	    void SOCI::executeSql(const std::string &sql)
	    {
		if (showQueries()) {
		    LOG_INFO(sql);
		}
		std::cerr << "executeSql " << sql << "\n";

		soci::statement st(impl_->session);
		st.alloc();
		st.prepare(sql);
		st.execute(true);
	    }

	    void SOCI::startTransaction()
	    {
		if (showQueries()) {
		    LOG_INFO("begin transaction -- implicit");
		}
	    }

	    void SOCI::commitTransaction()
	    {
		if (showQueries()) {
		    LOG_INFO("commit transaction -- using SQLEndTran");
		}
	    }

	    void SOCI::rollbackTransaction()
	    {
		if (showQueries()) {
		    LOG_INFO("rollback transaction -- using SQLEndTran");
		}
	    }

	    std::unique_ptr<SqlStatement> SOCI::prepareStatement(const std::string &sql)
	    {
		return std::unique_ptr<SqlStatement>(new SOCIStatement(*this, sql));
	    }

	    std::string SOCI::autoincrementSql() const
	    {
		return "autoincrement";
	    }

	    std::vector<std::string> SOCI::autoincrementCreateSequenceSql(const std::string &table, const std::string &id) const
	    {
		return std::vector<std::string>();
	    }

	    std::vector<std::string> SOCI::autoincrementDropSequenceSql(const std::string &table, const std::string &id) const
	    {
		return std::vector<std::string>();
	    }

	    std::string SOCI::autoincrementType() const
	    {
		return "integer";
	    }

	    std::string SOCI::autoincrementInsertInfix(const std::string &id) const
	    {
		return "";
	    }

	    std::string SOCI::autoincrementInsertSuffix(const std::string &id) const
	    {
		return "";
	    }

	    const char *SOCI::dateTimeType(SqlDateTimeType type) const
	    {
		if (type == SqlDateTimeType::Date)
		    return "date";
		if (type == SqlDateTimeType::Time)
		    return "bigint"; // SQL Server has no proper duration type, so store duration as number of milliseconds
		if (type == SqlDateTimeType::DateTime)
		    return "datetime2";
		return "";
	    }

	    bool SOCI::requireSubqueryAlias() const
	    {
		return true;
	    }

	    const char *SOCI::blobType() const
	    {
		return "blob not null";
	    }

	    
	    const char *SOCI::booleanType() const
	    {
		return "bool";
	    }
	    

	    bool SOCI::supportAlterTable() const
	    {
		return true;
	    }

	    LimitQuery SOCI::limitQueryMethod() const
	    {
		return LimitQuery::OffsetFetch;
	    }

	}
    }
}
