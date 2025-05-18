/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef quantlib_spread_discount_curve_hpp
#define quantlib_spread_discount_curve_hpp

#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <utility>

namespace QuantLib {

    //! Yield curve based on interpolation of discount factors applied as
    //! a spread to the base YieldTermStructure
    /*! The discount factors spread at any given date is interpolated
        between the input data.

        \note This term structure will remain linked to the original
              structure, i.e., any changes in the latter will be
              reflected in this structure as well.

        \ingroup yieldtermstructures
    */

    template <class Interpolator>
    class InterpolatedSpreadDiscountCurve
        : public YieldTermStructure,
          protected InterpolatedCurve<Interpolator> {
      public:
        InterpolatedSpreadDiscountCurve(
            Handle<YieldTermStructure> originalCurve,
            const std::vector<Date>& dates,
            const std::vector<DiscountFactor>& dfs,
            const Interpolator& interpolator = {});
        //! \name YieldTermStructure interface
        //@{
        DayCounter dayCounter() const override;
        Natural settlementDays() const override;
        Calendar calendar() const override;
        const Date& referenceDate() const override;
        Date maxDate() const override;
        //@}
        //@}
        //! \name other inspectors
        //@{
        const std::vector<Time>& times() const;
        const std::vector<Date>& dates() const;
        const std::vector<Real>& data() const;
        std::vector<std::pair<Date, Real>> nodes() const;
        //@}
      protected:
        explicit InterpolatedSpreadDiscountCurve(
            Handle<YieldTermStructure> originalCurve,
            const Interpolator& interpolator = {});
        //! \name YieldTermStructure implementation
        //@{
        DiscountFactor discountImpl(Time) const override;
        //@}
        void update() override;

        mutable std::vector<Date> dates_;
      private:
        void updateInterpolation();
        DiscountFactor calcSpread(Time t) const;

        Handle<YieldTermStructure> originalCurve_;
    };

    //! Spread yield curve based on log-linear interpolation of discount factors
    /*! Log-linear interpolation guarantees piecewise-constant forward
        rates.

        \ingroup yieldtermstructures
    */
    typedef InterpolatedSpreadDiscountCurve<LogLinear> SpreadDiscountCurve;


    // inline definitions

    template <class T>
    inline InterpolatedSpreadDiscountCurve<T>::InterpolatedSpreadDiscountCurve(
        Handle<YieldTermStructure> originalCurve,
        const std::vector<Date>& dates,
        const std::vector<DiscountFactor>& dfs,
        const Interpolator& interpolator)
    : InterpolatedCurve<T>({}, dfs, interpolator), dates_(dates),
      originalCurve_(std::move(originalCurve)) {
        QL_REQUIRE(dates_.size() >= T::requiredPoints,
                   "not enough input dates given");
        QL_REQUIRE(this->data_.size() == dates_.size(),
                   "dates/data count mismatch");
        for (auto df : this->data_) {
            QL_REQUIRE(df > 0.0, "negative discount");
        }

        registerWith(originalCurve_);
        if (!originalCurve_.empty())
            updateInterpolation();
    }

    template <class T>
    inline InterpolatedSpreadDiscountCurve<T>::InterpolatedSpreadDiscountCurve(
        Handle<YieldTermStructure> originalCurve,
        const Interpolator& interpolator)
    : InterpolatedCurve<T>(interpolator), originalCurve_(std::move(originalCurve))
    {
        registerWith(originalCurve_);
    }

    template <class T>
    inline DayCounter InterpolatedSpreadDiscountCurve<T>::dayCounter() const {
        return originalCurve_->dayCounter();
    }

    template <class T>
    inline Calendar InterpolatedSpreadDiscountCurve<T>::calendar() const {
        return originalCurve_->calendar();
    }

    template <class T>
    inline Natural InterpolatedSpreadDiscountCurve<T>::settlementDays() const {
        return originalCurve_->settlementDays();
    }

    template <class T>
    inline const Date&
    InterpolatedSpreadDiscountCurve<T>::referenceDate() const {
        return originalCurve_->referenceDate();
    }

    template <class T>
    inline Date InterpolatedSpreadDiscountCurve<T>::maxDate() const {
        Date maxDate = this->maxDate_ != Date() ? maxDate_ : dates_.back();
        return std::min(originalCurve_->maxDate(), maxDate);
    }

    template <class T>
    inline const std::vector<Time>&
    InterpolatedSpreadDiscountCurve<T>::times() const {
        return this->times_;
    }

    template <class T>
    inline const std::vector<Date>&
    InterpolatedSpreadDiscountCurve<T>::dates() const {
        return dates_;
    }

    template <class T>
    inline const std::vector<Real>&
    InterpolatedSpreadDiscountCurve<T>::data() const {
        return this->data_;
    }

    template <class T>
    inline std::vector<std::pair<Date, Real>>
    InterpolatedSpreadDiscountCurve<T>::nodes() const {
        std::vector<std::pair<Date, Real>> results(dates_.size());
        for (Size i=0, size=dates_.size(); i<size; ++i)
            results[i] = {dates_[i], this->data_[i]};
        return results;
    }

    template <class T>
    inline DiscountFactor
    InterpolatedSpreadDiscountCurve<T>::discountImpl(Time t) const {
        return originalCurve_->discount(t) * calcSpread(t);
    }

    template <class T>
    inline DiscountFactor
    InterpolatedSpreadDiscountCurve<T>::calcSpread(Time t) const {
        if (t <= this->times_.back())
            return this->interpolation_(t, true);

        // flat fwd extrapolation
        Time tMax = this->times_.back();
        DiscountFactor dMax = this->data_.back();
        Rate instFwdMax = - this->interpolation_.derivative(tMax) / dMax;
        return dMax * std::exp(- instFwdMax * (t-tMax));
    }

    template <class T>
    inline void InterpolatedSpreadDiscountCurve<T>::update() {
        if (!originalCurve_.empty()) {
            updateInterpolation();
            YieldTermStructure::update();
        } else {
            /* The implementation inherited from YieldTermStructure
               asks for our reference date, which we don't have since
               the original curve is still not set. Therefore, we skip
               over that and just call the base-class behavior. */
            // NOLINTNEXTLINE(bugprone-parent-virtual-call)
            TermStructure::update();
        }
    }

    template <class T>
    inline void InterpolatedSpreadDiscountCurve<T>::updateInterpolation() {
        this->setupTimes(dates_, referenceDate(), dayCounter());
        this->setupInterpolation();
        this->interpolation_.update();
    }

}

#endif
