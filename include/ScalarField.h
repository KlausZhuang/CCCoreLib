// SPDX-License-Identifier: LGPL-2.0-or-later
// Copyright © EDF R&D / TELECOM ParisTech (ENST-TSI)

#pragma once

//Local
#include "CCConst.h"
#include "CCShareable.h"

//System
#include <vector>
#include <string>

// VE-6644: Not sure why the compiler can't recognize the exported functions for linux_arm7 if exporting the class ScalarField or any of its methods.
//          So I just put all methods implementation in the header file for now.

namespace CCCoreLib
{
	//! A simple scalar field (to be associated to a point cloud)
	/** A monodimensional array of scalar values. It has also specific
		parameters for display purposes.

		It is now using a base offset value of type double, and internally
		stores the values as float to limit the memory consumption.

		Invalid values can be represented by CCCoreLib::NAN_VALUE.
	**/
	class ScalarField : public std::vector<float>, public CCShareable
	{
	public:

		//! Shortcut to the (protected) std::vector::size() method
		using std::vector<float>::size;
		//! Shortcut to the (protected) std::vector::capacity() method
		using std::vector<float>::capacity;
		//! Shortcut to the (protected) std::vector::reserve() method
		using std::vector<float>::reserve;
		//! Shortcut to the (protected) std::vector::shrink_to_fit() method
		using std::vector<float>::shrink_to_fit;
		//! Shortcut to the (protected) std::vector::empty() method
		using std::vector<float>::empty;

		//! Default constructor
		/** [SHAREABLE] Call 'link' when associating this structure to an object.
			\param name scalar field name
		**/
		explicit ScalarField(const std::string& name = std::string())
			: m_name{ }
			, m_offset{ 0.0 }
			, m_offsetHasBeenSet{ false }
			, m_localMinVal{ 0.0f }
			, m_localMaxVal{ 0.0f }
		{
			setName(name);
		}

		//! Copy constructor
		/** \param sf scalar field to copy
			\warning May throw a std::bad_alloc exception
		**/
		ScalarField(const ScalarField& sf)
			: std::vector<float>(sf)
			, m_name{ sf.m_name }
			, m_offset{ sf.m_offset }
			, m_offsetHasBeenSet{ sf.m_offsetHasBeenSet }
			, m_localMinVal{ sf.m_localMinVal }
			, m_localMaxVal{ sf.m_localMaxVal }
		{
		}

		//! Sets scalar field name
		void setName(const std::string& name)
		{
			if (name.empty())
			{
				m_name = "Undefined";
			}
			else
			{
				m_name = name;
			}
		}

		//! Returns scalar field name
		inline const std::string& getName() const { return m_name; }

		//! Returns the specific NaN value
		static inline ScalarType NaN() { return NAN_VALUE; }

		//! Returns the offset
		inline double getOffset() const { return m_offset; }

		//! Sets the offset
		/** \warning Dangerous. All values inside the vector are relative to the offset!
			\param offset the new offset value
		**/
		inline void setOffset(double offset)
		{
			assert(std::isfinite(offset));

			m_offsetHasBeenSet = true;
			m_offset = offset;
		}

		//! Resets the offset
		inline void resetOffset()
		{
			m_offsetHasBeenSet = false;
			m_offset = 0.0;
		}

		//! Clears the scalar field
		inline void clear()
		{
			std::vector<float>::clear();
			m_offsetHasBeenSet = false;
		}

		//! Computes the mean value (and optionally the variance value) of the scalar field
		/** \param mean a field to store the mean value
			\param variance if not void, the variance will be computed and stored here
		**/
		void computeMeanAndVariance(ScalarType& mean, ScalarType* variance = nullptr) const
		{
			double _mean = 0.0;
			double _std2 = 0.0;
			std::size_t count = 0;

			for (std::size_t i = 0; i < size(); ++i)
			{
				float val = at(i);
				if (std::isfinite(val))
				{
					_mean += val;
					_std2 += static_cast<double>(val) * val;
					++count;
				}
			}

			if (count)
			{
				_mean /= count;
				mean = _mean;

				if (variance)
				{
					_std2 = std::abs(_std2 / count - _mean * _mean);
					*variance = static_cast<ScalarType>(_std2);
				}

				mean += m_offset; // only after the standard deviation has been calculated!

			}
			else
			{
				mean = 0;
				if (variance)
				{
					*variance = 0;
				}
			}
		}

		//! Determines the min and max values
		virtual void computeMinAndMax();

		//! Returns whether a scalar value is valid or not
		static inline bool ValidValue(ScalarType value) { return std::isfinite(value); }

		//! Sets the value as 'invalid' (i.e. CCCoreLib::NAN_VALUE)
		inline void flagValueAsInvalid(std::size_t index) { (*this)[index] = std::numeric_limits<float>::quiet_NaN(); }

		//! Returns the number of valid values in this scalar field
		std::size_t countValidValues() const
		{
			if (false == std::isfinite(m_offset))
			{
				// special case: if the offset is invalid, all values become invalid!
				return size();
			}

			std::size_t count = 0;

			for (std::size_t i = 0; i < size(); ++i)
			{
				const ScalarType& val = at(i);
				if (ValidValue(val))
				{
					++count;
				}
			}

			return count;
		}

		//! Returns the minimum value
		inline ScalarType getMin() const { return m_offset + m_localMinVal; }
		//! Returns the maximum value
		inline ScalarType getMax() const { return m_offset + m_localMaxVal; }

		//! Fills the array with a particular value
		inline void fill(ScalarType fillValue = 0)
		{
			float fillValueF = 0.0f;
			if (std::isfinite(fillValue))
			{
				if (m_offsetHasBeenSet)
				{
					fillValueF = static_cast<float>(fillValue - m_offset);
				}
				else
				{
					// if the offset has not been set yet, we use the first finite value by default
					setOffset(fillValue);

					//fillValueF = 0.0f; // already set
				}
			}
			else
			{
				// special case: filling with NaN or +/-inf values
				// (it doesn't really give an idea of what the optimal offset is)
				resetOffset();

				fillValueF = static_cast<float>(fillValue); // NaN/-inf/+inf should be maintained
			}

			if (empty())
			{
				resize(capacity(), fillValueF);
			}
			else
			{
				std::fill(begin(), end(), fillValueF);
			}
		}

		//! Reserves memory (no exception thrown)
		bool reserveSafe(std::size_t count)
		{
			try
			{
				reserve(count);
			}
			catch (const std::bad_alloc&)
			{
				//not enough memory
				return false;
			}
			return true;
		}
		//! Resizes memory (no exception thrown)
		bool resizeSafe(std::size_t count, bool initNewElements = false, ScalarType valueForNewElements = 0)
		{
			try
			{
				if (initNewElements && count > size())
				{
					float fillValueF = 0.0f;
					if (std::isfinite(valueForNewElements))
					{
						if (m_offsetHasBeenSet)
						{
							// use the already set offset
							fillValueF = static_cast<float>(valueForNewElements - m_offset);
						}
						else // if the offset has not been set yet...
						{
							// we use the first finite value as offset by default
							setOffset(valueForNewElements);
						}
					}
					else
					{
						// special case: filling with NaN or +/-inf values
						fillValueF = static_cast<float>(valueForNewElements); // NaN/-inf/+inf should be maintained
					}

					resize(count, fillValueF);
				}
				else
				{
					resize(count);
				}
			}
			catch (const std::bad_alloc&)
			{
				//not enough memory
				return false;
			}
			return true;
		}

		//Shortcuts (for backward compatibility)
		inline ScalarType getValue(std::size_t index) const { return m_offset + (*this)[index]; }

		inline float getLocalValue(std::size_t index) const { return (*this)[index]; }
		inline void setLocalValue(std::size_t index, float value) { (*this)[index] = value; }
		inline const float* getLocalValues() const { return data(); }

		inline void setValue(std::size_t index, ScalarType value)
		{
			if (m_offsetHasBeenSet)
			{
				// use the already set offset
				(*this)[index] = static_cast<float>(value - m_offset);
			}
			else if (std::isfinite(value))
			{
				// if the offset has not been set yet, we use the
				// first finite value as offset by default
				setOffset(value);
				(*this)[index] = 0.0f;
			}
			else
			{
				// we can't set an offset
				(*this)[index] = static_cast<float>(value); // NaN/-inf/+inf should be maintained
			}
		}

		inline void addElement(ScalarType value)
		{
			if (m_offsetHasBeenSet)
			{
				// use the already set offset
				push_back(static_cast<float>(value - m_offset));
			}
			else if (std::isfinite(value))
			{
				// if the offset has not been set yet, we use the
				// first finite value as offset by default
				setOffset(value);
				push_back(0.0f);
			}
			else
			{
				// we can't set an offset
				push_back(static_cast<float>(value)); // NaN/-inf/+inf should be maintained
			}
		}

		inline unsigned currentSize() const { return static_cast<unsigned>(size()); }

		inline void swap(std::size_t i1, std::size_t i2) { std::swap(at(i1), at(i2)); }

	protected: //methods

		//! Default destructor
		/** Call release instead.
		**/
		~ScalarField() override = default;

	protected: //members

		//! Scalar field name
		std::string m_name;

	private:
		//! Offset value (local to global)
		double m_offset;
		//! Whether the offset has been set or not
		bool m_offsetHasBeenSet;
		//! Minimum value (local)
		float m_localMinVal;
		//! Maximum value (local)
		float m_localMaxVal;
	};

	inline void ScalarField::computeMinAndMax()
	{
		float localMinVal = 0.0f;
		float localMaxVal = 0.0f;

		bool minMaxInitialized = false;
		for (std::size_t i = 0; i < size(); ++i)
		{
			float val = at(i);
			if (std::isfinite(val))
			{
				if (minMaxInitialized)
				{
					if (val < localMinVal)
						localMinVal = val;
					else if (val > localMaxVal)
						localMaxVal = val;
				}
				else
				{
					//first valid value is used to init min and max
					localMinVal = localMaxVal = val;
					minMaxInitialized = true;
				}
			}
		}

		if (minMaxInitialized)
		{
			m_localMinVal = localMinVal;
			m_localMaxVal = localMaxVal;
		}
		else //particular case: zero valid values
		{
			m_localMinVal = m_localMaxVal = 0.0;
		}
	}
}
