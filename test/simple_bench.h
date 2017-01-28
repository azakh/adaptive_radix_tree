#pragma once

#include "gtest/gtest.h"

#include <chrono>

class AutoReportTimeProperty
{
	typedef std::chrono::steady_clock performance_clock;
	const char* property_name_;
	performance_clock::time_point start_;
	bool started_;
public:
	AutoReportTimeProperty(const char* property_name)
		: property_name_(property_name)
		, started_(false)
	{
	}

	bool Next()
	{
		if (!started_)
		{
			started_ = true;
			start_ = performance_clock::now();
			return true;
		}

		int64_t elapsed_ns = std::chrono::duration<int64_t, std::nano>(performance_clock::now() - start_).count();
		::testing::Test::RecordProperty(property_name_, elapsed_ns / 1000000LL);
		started_ = false;
		return false;
	}
};

#define TIME_AUTO(propertyName) for (AutoReportTimeProperty auto_report_time_property(propertyName); auto_report_time_property.Next();)
