#include <pep/structure/ColumnNameSerializers.hpp>
#include <pep/serialization/Serialization.hpp>

namespace pep {

void Serializer<ColumnNameSection>::moveIntoProtocolBuffer(proto::ColumnNameSection& dest, ColumnNameSection value) const {
  dest.set_value(value.getValue());
}

ColumnNameSection Serializer<ColumnNameSection>::fromProtocolBuffer(proto::ColumnNameSection&& source) const {
  return ColumnNameSection(std::move(*source.mutable_value()));
}

void Serializer<ColumnNameMapping>::moveIntoProtocolBuffer(proto::ColumnNameMapping& dest, ColumnNameMapping value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_original(), value.original);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_mapped(), value.mapped);
}

ColumnNameMapping Serializer<ColumnNameMapping>::fromProtocolBuffer(proto::ColumnNameMapping&& source) const {
  return ColumnNameMapping{ Serialization::FromProtocolBuffer(std::move(*source.mutable_original())), Serialization::FromProtocolBuffer(std::move(*source.mutable_mapped())) };
}

}
