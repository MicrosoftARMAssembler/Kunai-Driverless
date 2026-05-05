#pragma once
namespace game {
    namespace math {
        f_matrix to_rotation_matrix( f_rotator rotation ) {
            f_matrix matrix = {};
            auto rad_pitch = ( rotation.pitch * std::numbers::pi / 180.f );
            auto rad_yaw = ( rotation.yaw * std::numbers::pi / 180.f );
            auto rad_roll = ( rotation.roll * std::numbers::pi / 180.f );
            auto sin_pitch = sin( rad_pitch );
            auto cos_pitch = cos( rad_pitch );
            auto sin_yaw = sin( rad_yaw );
            auto cos_yaw = cos( rad_yaw );
            auto sin_roll = sin( rad_roll );
            auto cos_roll = cos( rad_roll );
            matrix.x_plane.x = cos_pitch * cos_yaw;
            matrix.x_plane.y = cos_pitch * sin_yaw;
            matrix.x_plane.z = sin_pitch;
            matrix.x_plane.w = 0.f;
            matrix.y_plane.x = sin_roll * sin_pitch * cos_yaw - cos_roll * sin_yaw;
            matrix.y_plane.y = sin_roll * sin_pitch * sin_yaw + cos_roll * cos_yaw;
            matrix.y_plane.z = -sin_roll * cos_pitch;
            matrix.y_plane.w = 0.f;
            matrix.z_plane.x = -( cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw );
            matrix.z_plane.y = cos_yaw * sin_roll - cos_roll * sin_pitch * sin_yaw;
            matrix.z_plane.z = cos_roll * cos_pitch;
            matrix.z_plane.w = 0.f;
            matrix.w_plane.w = 1.f;
            return matrix;
        }

        bool in_rect(
            double radius,
            f_vector2d screen_position
        ) {
            return ( kunai::screen_w / 2.f ) >= ( ( kunai::screen_w / 2.f ) - radius ) && ( kunai::screen_w / 2.f ) <= ( ( kunai::screen_w / 2.f ) + radius ) &&
                screen_position.y >= ( screen_position.y - radius ) && screen_position.y <= ( screen_position.y + radius );
        }

        bool in_circle(
            double radius,
            f_vector2d screen_position
        ) {
            if ( in_rect( radius, screen_position ) ) {
                auto dx = ( kunai::screen_w / 2.f ) - screen_position.x; dx *= dx;
                auto dy = ( kunai::screen_h / 2.f ) - screen_position.y; dy *= dy;
                return dx + dy <= radius * radius;
            } return false;
        }
    }

    f_vector2d world_to_screen( const f_vector& world_location, const std::shared_ptr<const cache::camera_t>& camera ) {
        auto delta = world_location - camera->location;

        const double dist = std::sqrt(
            static_cast< double >( delta.x ) * delta.x +
            static_cast< double >( delta.y ) * delta.y +
            static_cast< double >( delta.z ) * delta.z
        );
        if ( dist <= 0.0 || dist > 10000000.0 )
            return f_vector2d( -1.f, -1.f );

        auto matrix = math::to_rotation_matrix( camera->rotation );

        const auto axis_x = f_vector( matrix.x_plane.x, matrix.x_plane.y, matrix.x_plane.z );
        const auto axis_y = f_vector( matrix.y_plane.x, matrix.y_plane.y, matrix.y_plane.z );
        const auto axis_z = f_vector( matrix.z_plane.x, matrix.z_plane.y, matrix.z_plane.z );

        const auto transform = f_vector(
            delta.vector_scalar( axis_y ),
            delta.vector_scalar( axis_z ),
            delta.vector_scalar( axis_x )
        );

        if ( transform.z < 1.f )
            return f_vector2d( -1.f, -1.f );

        double fov = static_cast< double >( camera->fov );
        if ( fov <= 0.0 || fov >= 180.0 )
            fov = 103.0;

        const double fov_factor = static_cast< double >( kunai::center_w )
            / std::tan( fov * std::numbers::pi / 360.0 );

        const double out_x = kunai::center_w + ( transform.x / transform.z ) * fov_factor;
        const double out_y = kunai::center_h - ( transform.y / transform.z ) * fov_factor;

        if ( out_x != out_x || out_y != out_y )
            return f_vector2d( -1.f, -1.f );

        const double w = static_cast< double >( kunai::center_w ) * 2.0;
        const double h = static_cast< double >( kunai::center_h ) * 2.0;
        if ( out_x < -w || out_x > w * 2.0 || out_y < -h || out_y > h * 2.0 )
            return f_vector2d( -1.f, -1.f );

        return f_vector2d(
            static_cast< float >( out_x ),
            static_cast< float >( out_y )
        );
    }

    f_rotator find_look_at_rotation( const f_vector& start, const f_vector& target ) {
        const f_vector delta = target - start;

        const double dist_xy = std::sqrt( delta.x * delta.x + delta.y * delta.y );
        const double pitch = std::atan2( delta.z, dist_xy ) * ( 180.0 / std::numbers::pi );
        const double yaw = std::atan2( delta.y, delta.x ) * ( 180.0 / std::numbers::pi );

        return f_rotator( pitch, yaw, 0.0 );
    }
}