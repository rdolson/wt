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
		}
		
		Impl(const Impl &other)
		    : connectionString(other.connectionString) {
		    resultBuffer.buf = (char*)malloc(256);
		    resultBuffer.size = 256;
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
		SOCIStatement(SOCI &conn, const std::string &sql)
		    : stmt_(conn.impl_->session),
		      sql_(sql),
		      lastId_(-1) {

		    std::cerr << "STMT: " << sql << "\n";
		    }

		virtual ~SOCIStatement()
		{
		}
		
		
		virtual void reset() override
		{
		}
		
		virtual void bind(int column, const std::string &value) override
		{
		    std::cerr << "BIND STR " << column << " " << value << "\n";
		    stmt_.exchange(soci::use(value));
		}
		
		virtual void bind(int column, short value) override
		{
		    std::cerr << "BIND SHORT " << column << " " << value << "\n";
		    stmt_.exchange(soci::use(value));
		}

		virtual void bind(int column, int value) override
		{
		    std::cerr << "BIND INT " << column << " " << value << "\n";
		    auto *y = new soci::details::use_container<int, void>(soci::use(value));
		    std::cerr << "created " << y << "\n";
		    stmt_.exchange(*y);
		}

		virtual void bind(int column, long long value) override
		{
		    std::cerr << "BIND LL " << column << " " << value << "\n";
		    stmt_.exchange(soci::use(value));
		}
		
		virtual void bind(int column, float value) override
		{
		    std::cerr << "BIND FLOAT " << column << " " << value << "\n";
		    stmt_.exchange(soci::use(static_cast<double>(value)));
		}

		virtual void bind(int column, double value) override
		{
		    std::cerr << "BIND DOUBLE " << column << " " << value << "\n";
		    stmt_.exchange(soci::use(value));
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
		}

		virtual void execute() override
		{
		    std::cerr<< "EXECUTE\n";
		    stmt_.alloc();
		    stmt_.prepare(sql_);
		    stmt_.define_and_bind();
		    stmt_.execute(true);
		}

		virtual long long insertedId() override
		{
		    return lastId_;
		}
		
		virtual int affectedRowCount() override
		{
		    return 0;
		    // return static_cast<int>(affectedRows_);
		}

		virtual bool nextRow() override
		{
		    return false;
		}

		virtual int columnCount() const override
		{
		    return 0;
		}
		
		virtual bool getResult(int column, std::string *value, int /*size*/) override
		{
		    return false;
		}

		virtual bool getResult(int column, short * value) override
		{
		    return false;
		}

		virtual bool getResult(int column, int * value) override
		{
		    return false;
		}

		virtual bool getResult(int column, long long * value) override
		{
		    return false;
		}

		virtual bool getResult(int column, float * value) override
		{
		    return false;
		}

		virtual bool getResult(int column, double * value) override
		{
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
		std::string sql_;
		long long lastId_;
		
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
